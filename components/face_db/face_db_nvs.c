#include "face_db_nvs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "face_db_nvs";

#define FACE_NAMESPACE "faces"
/* Bump when the record layout changes; older records are then skipped. */
#define RECORD_FORMAT_VERSION 1u

/* Stored blob: this header followed by embedding_len floats. */
typedef struct __attribute__((packed)) {
  uint16_t format_version;
  uint16_t embedding_len;
  char name[FACE_DB_NAME_MAX_LEN + 1];
} record_header_t;

typedef struct {
  nvs_handle_t handle;
  SemaphoreHandle_t mutex;
} nvs_backend_t;

static nvs_backend_t s_backend;

static void record_key(uint16_t id, char key[NVS_KEY_NAME_MAX_SIZE]) {
  snprintf(key, NVS_KEY_NAME_MAX_SIZE, "f%05u", (unsigned)id);
}

static face_db_status_t load_one(nvs_backend_t *backend, face_db_t *db,
                                 face_db_add_record_fn add_record,
                                 const char *key) {
  unsigned long id = strtoul(key + 1, NULL, 10);
  if (key[0] != 'f' || id == 0 || id > UINT16_MAX) {
    ESP_LOGW(TAG, "Skipping unexpected key '%s'", key);
    return FACE_DB_OK;
  }
  size_t blob_size = 0;
  if (nvs_get_blob(backend->handle, key, NULL, &blob_size) != ESP_OK ||
      blob_size < sizeof(record_header_t)) {
    ESP_LOGW(TAG, "Skipping unreadable record '%s'", key);
    return FACE_DB_OK;
  }
  uint8_t *blob = malloc(blob_size);
  if (blob == NULL) {
    return FACE_DB_ERR_NO_MEM;
  }
  face_db_status_t status = FACE_DB_OK;
  record_header_t header;
  if (nvs_get_blob(backend->handle, key, blob, &blob_size) == ESP_OK) {
    memcpy(&header, blob, sizeof(header));
    header.name[FACE_DB_NAME_MAX_LEN] = '\0';
    const size_t expected_size =
        sizeof(header) + (size_t)header.embedding_len * sizeof(float);
    if (header.format_version != RECORD_FORMAT_VERSION ||
        blob_size != expected_size) {
      ESP_LOGW(TAG, "Skipping record '%s': format %u, %u bytes", key,
               header.format_version, (unsigned)blob_size);
    } else {
      /* The floats may be unaligned inside the blob: copy them out. */
      float *embedding = malloc((size_t)header.embedding_len * sizeof(float));
      if (embedding == NULL) {
        status = FACE_DB_ERR_NO_MEM;
      } else {
        memcpy(embedding, blob + sizeof(header),
               (size_t)header.embedding_len * sizeof(float));
        const face_db_record_t record = {.id = (uint16_t)id,
                                         .name = header.name,
                                         .embedding = embedding,
                                         .embedding_len = header.embedding_len};
        status = add_record(db, &record);
        free(embedding);
      }
    }
  }
  free(blob);
  return status;
}

static face_db_status_t nvs_load_all(void *context, face_db_t *db,
                                     face_db_add_record_fn add_record) {
  nvs_backend_t *backend = context;
  nvs_iterator_t iterator = NULL;
  esp_err_t err = nvs_entry_find(CONFIG_FACE_DB_NVS_PARTITION, FACE_NAMESPACE,
                                 NVS_TYPE_BLOB, &iterator);
  face_db_status_t status = FACE_DB_OK;
  while (err == ESP_OK && status == FACE_DB_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(iterator, &info);
    status = load_one(backend, db, add_record, info.key);
    err = nvs_entry_next(&iterator);
  }
  nvs_release_iterator(iterator);
  if (status == FACE_DB_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "Reading faces failed: %s", esp_err_to_name(err));
    return FACE_DB_ERR_STORAGE;
  }
  return status;
}

static face_db_status_t nvs_save(void *context,
                                 const face_db_record_t *record) {
  nvs_backend_t *backend = context;
  const size_t embedding_size = record->embedding_len * sizeof(float);
  const size_t blob_size = sizeof(record_header_t) + embedding_size;
  uint8_t *blob = malloc(blob_size);
  if (blob == NULL) {
    return FACE_DB_ERR_NO_MEM;
  }
  record_header_t header = {.format_version = RECORD_FORMAT_VERSION,
                            .embedding_len = (uint16_t)record->embedding_len};
  strncpy(header.name, record->name, FACE_DB_NAME_MAX_LEN);
  memcpy(blob, &header, sizeof(header));
  memcpy(blob + sizeof(header), record->embedding, embedding_size);

  char key[NVS_KEY_NAME_MAX_SIZE];
  record_key(record->id, key);
  esp_err_t err = nvs_set_blob(backend->handle, key, blob, blob_size);
  if (err == ESP_OK) {
    err = nvs_commit(backend->handle);
  }
  free(blob);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Saving face %u failed: %s", record->id,
             esp_err_to_name(err));
    return FACE_DB_ERR_STORAGE;
  }
  return FACE_DB_OK;
}

static face_db_status_t nvs_erase(void *context, uint16_t id) {
  nvs_backend_t *backend = context;
  char key[NVS_KEY_NAME_MAX_SIZE];
  record_key(id, key);
  esp_err_t err = nvs_erase_key(backend->handle, key);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = ESP_OK; /* already gone is fine */
  }
  if (err == ESP_OK) {
    err = nvs_commit(backend->handle);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Erasing face %u failed: %s", id, esp_err_to_name(err));
    return FACE_DB_ERR_STORAGE;
  }
  return FACE_DB_OK;
}

static face_db_status_t nvs_erase_everything(void *context) {
  nvs_backend_t *backend = context;
  esp_err_t err = nvs_erase_all(backend->handle);
  if (err == ESP_OK) {
    err = nvs_commit(backend->handle);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Erasing all faces failed: %s", esp_err_to_name(err));
    return FACE_DB_ERR_STORAGE;
  }
  return FACE_DB_OK;
}

static void mutex_lock(void *context) {
  xSemaphoreTake(((nvs_backend_t *)context)->mutex, portMAX_DELAY);
}

static void mutex_unlock(void *context) {
  xSemaphoreGive(((nvs_backend_t *)context)->mutex);
}

static esp_err_t mount_partition(void) {
  esp_err_t err = nvs_flash_init_partition(CONFIG_FACE_DB_NVS_PARTITION);
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "Face partition unusable (%s), erasing it",
             esp_err_to_name(err));
    ESP_RETURN_ON_ERROR(nvs_flash_erase_partition(CONFIG_FACE_DB_NVS_PARTITION),
                        TAG, "erase failed");
    err = nvs_flash_init_partition(CONFIG_FACE_DB_NVS_PARTITION);
  }
  return err;
}

esp_err_t face_db_nvs_open(size_t embedding_len, face_db_t **out_db) {
  ESP_RETURN_ON_FALSE(out_db != NULL && embedding_len > 0, ESP_ERR_INVALID_ARG,
                      TAG, "bad argument");
  ESP_RETURN_ON_FALSE(embedding_len <= UINT16_MAX, ESP_ERR_INVALID_ARG, TAG,
                      "embedding too long");
  ESP_RETURN_ON_FALSE(s_backend.mutex == NULL, ESP_ERR_INVALID_STATE, TAG,
                      "already open");

  ESP_RETURN_ON_ERROR(mount_partition(), TAG, "mount '%s' failed",
                      CONFIG_FACE_DB_NVS_PARTITION);
  ESP_RETURN_ON_ERROR(nvs_open_from_partition(CONFIG_FACE_DB_NVS_PARTITION,
                                              FACE_NAMESPACE, NVS_READWRITE,
                                              &s_backend.handle),
                      TAG, "open namespace failed");
  s_backend.mutex = xSemaphoreCreateMutex();
  if (s_backend.mutex == NULL) {
    nvs_close(s_backend.handle);
    return ESP_ERR_NO_MEM;
  }

  const face_db_storage_t storage = {.context = &s_backend,
                                     .load_all = nvs_load_all,
                                     .save = nvs_save,
                                     .erase = nvs_erase,
                                     .erase_all = nvs_erase_everything};
  const face_db_lock_t lock = {
      .context = &s_backend, .lock = mutex_lock, .unlock = mutex_unlock};
  const face_db_config_t config = {.max_faces = CONFIG_FACE_DB_MAX_FACES,
                                   .embedding_len = embedding_len,
                                   .storage = &storage,
                                   .lock = &lock};
  face_db_status_t status = face_db_create(&config, out_db);
  if (status != FACE_DB_OK) {
    ESP_LOGE(TAG, "Creating face store failed: %s",
             face_db_status_name(status));
    vSemaphoreDelete(s_backend.mutex);
    s_backend.mutex = NULL;
    nvs_close(s_backend.handle);
    return status == FACE_DB_ERR_NO_MEM ? ESP_ERR_NO_MEM : ESP_FAIL;
  }
  ESP_LOGI(TAG, "Loaded %u face(s), skipped %u, capacity %u",
           (unsigned)face_db_count(*out_db),
           (unsigned)face_db_skipped_count(*out_db),
           (unsigned)face_db_capacity(*out_db));
  return ESP_OK;
}

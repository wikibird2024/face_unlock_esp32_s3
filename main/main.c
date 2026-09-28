#include "ai_face_recognition.h"
#include "camera_driver.h"
#include "console_commands.h"
#include "door_control.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "face_db_nvs.h"
#include "net_comm.h"
#include "nvs_flash.h"
#include "system_logic.h"

static const char *TAG = "app_main";

static esp_err_t init_default_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "Default NVS unusable (%s), erasing", esp_err_to_name(err));
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
    err = nvs_flash_init();
  }
  return err;
}

/* Model first, then face store (needs the embedding length), then camera,
 * so the console can still list/delete faces when the camera is broken. */
static esp_err_t start_recognition(face_db_t **out_face_db) {
  ESP_RETURN_ON_ERROR(ai_face_recognition_init(), TAG, "face model");
  ESP_RETURN_ON_ERROR(
      face_db_nvs_open(ai_face_recognition_embedding_len(), out_face_db), TAG,
      "face store");
  ESP_RETURN_ON_ERROR(camera_driver_init(), TAG, "camera");
  ESP_RETURN_ON_ERROR(system_logic_start(*out_face_db), TAG, "system logic");
  return ESP_OK;
}

void app_main(void) {
  const esp_app_desc_t *app = esp_app_get_description();
  ESP_LOGI(TAG, "Face unlock %s (IDF %s)", app->version, app->idf_ver);

  ESP_ERROR_CHECK(init_default_nvs());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  /* Lock output first: the door must be locked whatever fails later. */
  ESP_ERROR_CHECK(door_control_init());
  ESP_ERROR_CHECK(net_comm_init());

  face_db_t *face_db = NULL;
  esp_err_t err = start_recognition(&face_db);
  if (err != ESP_OK) {
    ESP_LOGE(TAG,
             "Recognition NOT running (%s). Door stays locked; console "
             "is still available for diagnosis.",
             esp_err_to_name(err));
  }

  ESP_ERROR_CHECK(console_commands_start(face_db));
  ESP_LOGI(TAG, "Ready. Type 'help' in the console.");
}

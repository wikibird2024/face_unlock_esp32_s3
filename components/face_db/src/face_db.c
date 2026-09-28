#include "face_db.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint16_t id;
  char name[FACE_DB_NAME_MAX_LEN + 1];
} face_slot_t;

struct face_db {
  size_t max_faces;
  size_t embedding_len;
  size_t count;
  size_t skipped_count;
  bool has_storage;
  bool has_lock;
  face_db_storage_t storage;
  face_db_lock_t lock;
  /* Faces are packed at the front: slots[0 .. count-1]. */
  face_slot_t *slots;
  /* embeddings[i * embedding_len ..] belongs to slots[i]. */
  float *embeddings;
  /* Scratch buffer for the normalised query embedding. */
  float *query;
};

static void db_lock(face_db_t *db) {
  if (db->has_lock) {
    db->lock.lock(db->lock.context);
  }
}

static void db_unlock(face_db_t *db) {
  if (db->has_lock) {
    db->lock.unlock(db->lock.context);
  }
}

static float *embedding_at(const face_db_t *db, size_t index) {
  return &db->embeddings[index * db->embedding_len];
}

/* Copy `source` into `target` scaled to length 1. Fails on a zero vector. */
static bool normalise(const float *source, float *target, size_t len) {
  double sum = 0.0;
  for (size_t i = 0; i < len; i++) {
    sum += (double)source[i] * (double)source[i];
  }
  if (!(sum > 0.0) || !isfinite(sum)) {
    return false;
  }
  const float scale = (float)(1.0 / sqrt(sum));
  for (size_t i = 0; i < len; i++) {
    target[i] = source[i] * scale;
  }
  return true;
}

static float dot_product(const float *a, const float *b, size_t len) {
  float sum = 0.0f;
  for (size_t i = 0; i < len; i++) {
    sum += a[i] * b[i];
  }
  return sum;
}

static void copy_name(char *target, const char *source) {
  strncpy(target, source, FACE_DB_NAME_MAX_LEN);
  target[FACE_DB_NAME_MAX_LEN] = '\0';
}

static long find_index(const face_db_t *db, uint16_t id) {
  for (size_t i = 0; i < db->count; i++) {
    if (db->slots[i].id == id) {
      return (long)i;
    }
  }
  return -1;
}

/* Next id: one above the highest in use, or the lowest free one after the
 * highest possible id has been used. */
static uint16_t next_free_id(const face_db_t *db) {
  uint16_t highest = FACE_DB_NO_ID;
  for (size_t i = 0; i < db->count; i++) {
    if (db->slots[i].id > highest) {
      highest = db->slots[i].id;
    }
  }
  if (highest < UINT16_MAX) {
    return (uint16_t)(highest + 1u);
  }
  for (uint32_t id = 1; id <= UINT16_MAX; id++) {
    if (find_index(db, (uint16_t)id) < 0) {
      return (uint16_t)id;
    }
  }
  return FACE_DB_NO_ID;
}

/* Put a record in the next free slot. Caller holds the lock. */
static face_db_status_t insert_record(face_db_t *db, uint16_t id,
                                      const char *name,
                                      const float *embedding) {
  if (db->count >= db->max_faces) {
    return FACE_DB_ERR_FULL;
  }
  float *target = embedding_at(db, db->count);
  if (!normalise(embedding, target, db->embedding_len)) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  db->slots[db->count].id = id;
  copy_name(db->slots[db->count].name, name);
  db->count++;
  return FACE_DB_OK;
}

static void remove_index(face_db_t *db, size_t index) {
  const size_t last = db->count - 1;
  if (index != last) {
    db->slots[index] = db->slots[last];
    memcpy(embedding_at(db, index), embedding_at(db, last),
           db->embedding_len * sizeof(float));
  }
  db->count--;
}

face_db_status_t face_db_create(const face_db_config_t *config,
                                face_db_t **out_db) {
  if (config == NULL || out_db == NULL || config->max_faces == 0 ||
      config->embedding_len == 0) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  if (config->storage != NULL &&
      (config->storage->load_all == NULL || config->storage->save == NULL ||
       config->storage->erase == NULL || config->storage->erase_all == NULL)) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  if (config->lock != NULL &&
      (config->lock->lock == NULL || config->lock->unlock == NULL)) {
    return FACE_DB_ERR_INVALID_ARG;
  }

  face_db_t *db = calloc(1, sizeof(*db));
  if (db == NULL) {
    return FACE_DB_ERR_NO_MEM;
  }
  db->max_faces = config->max_faces;
  db->embedding_len = config->embedding_len;
  db->slots = calloc(config->max_faces, sizeof(face_slot_t));
  db->embeddings =
      calloc(config->max_faces * config->embedding_len, sizeof(float));
  db->query = calloc(config->embedding_len, sizeof(float));
  if (db->slots == NULL || db->embeddings == NULL || db->query == NULL) {
    face_db_destroy(db);
    return FACE_DB_ERR_NO_MEM;
  }
  if (config->storage != NULL) {
    db->storage = *config->storage;
    db->has_storage = true;
  }
  if (config->lock != NULL) {
    db->lock = *config->lock;
    db->has_lock = true;
  }

  if (db->has_storage) {
    face_db_status_t status = db->storage.load_all(db->storage.context, db,
                                                   face_db_add_loaded_record);
    if (status != FACE_DB_OK) {
      face_db_destroy(db);
      return status;
    }
  }
  *out_db = db;
  return FACE_DB_OK;
}

void face_db_destroy(face_db_t *db) {
  if (db == NULL) {
    return;
  }
  free(db->slots);
  free(db->embeddings);
  free(db->query);
  free(db);
}

face_db_status_t face_db_add_loaded_record(face_db_t *db,
                                           const face_db_record_t *record) {
  if (db == NULL || record == NULL || record->embedding == NULL ||
      record->name == NULL || record->id == FACE_DB_NO_ID) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  /* A record from another model or a duplicate id is skipped, not fatal:
   * one bad record must not lock everybody out. */
  if (record->embedding_len != db->embedding_len ||
      find_index(db, record->id) >= 0) {
    db->skipped_count++;
    return FACE_DB_OK;
  }
  face_db_status_t status =
      insert_record(db, record->id, record->name, record->embedding);
  if (status == FACE_DB_ERR_INVALID_ARG) {
    db->skipped_count++;
    return FACE_DB_OK;
  }
  return status;
}

face_db_status_t face_db_enroll(face_db_t *db, const char *name,
                                const float *embedding, size_t embedding_len,
                                uint16_t *out_id) {
  if (db == NULL || name == NULL || name[0] == '\0' || embedding == NULL ||
      embedding_len != db->embedding_len) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  db_lock(db);
  face_db_status_t status = FACE_DB_OK;
  const uint16_t id = next_free_id(db);
  if (id == FACE_DB_NO_ID) {
    status = FACE_DB_ERR_FULL;
  } else {
    status = insert_record(db, id, name, embedding);
  }
  if (status == FACE_DB_OK && db->has_storage) {
    const size_t index = db->count - 1;
    const face_db_record_t record = {
        .id = id,
        .name = db->slots[index].name,
        .embedding = embedding_at(db, index),
        .embedding_len = db->embedding_len,
    };
    if (db->storage.save(db->storage.context, &record) != FACE_DB_OK) {
      remove_index(db, index);
      status = FACE_DB_ERR_STORAGE;
    }
  }
  db_unlock(db);
  if (status == FACE_DB_OK && out_id != NULL) {
    *out_id = id;
  }
  return status;
}

face_db_status_t face_db_delete(face_db_t *db, uint16_t id) {
  if (db == NULL || id == FACE_DB_NO_ID) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  db_lock(db);
  face_db_status_t status = FACE_DB_OK;
  const long index = find_index(db, id);
  if (index < 0) {
    status = FACE_DB_ERR_NOT_FOUND;
  } else if (db->has_storage &&
             db->storage.erase(db->storage.context, id) != FACE_DB_OK) {
    status = FACE_DB_ERR_STORAGE;
  } else {
    remove_index(db, (size_t)index);
  }
  db_unlock(db);
  return status;
}

face_db_status_t face_db_clear(face_db_t *db) {
  if (db == NULL) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  db_lock(db);
  face_db_status_t status = FACE_DB_OK;
  if (db->has_storage &&
      db->storage.erase_all(db->storage.context) != FACE_DB_OK) {
    status = FACE_DB_ERR_STORAGE;
  } else {
    db->count = 0;
  }
  db_unlock(db);
  return status;
}

size_t face_db_count(face_db_t *db) {
  if (db == NULL) {
    return 0;
  }
  db_lock(db);
  const size_t count = db->count;
  db_unlock(db);
  return count;
}

size_t face_db_capacity(const face_db_t *db) {
  return db == NULL ? 0 : db->max_faces;
}

size_t face_db_embedding_len(const face_db_t *db) {
  return db == NULL ? 0 : db->embedding_len;
}

size_t face_db_skipped_count(const face_db_t *db) {
  return db == NULL ? 0 : db->skipped_count;
}

face_db_status_t face_db_get_entry(face_db_t *db, size_t index,
                                   face_db_entry_t *out_entry) {
  if (db == NULL || out_entry == NULL) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  db_lock(db);
  face_db_status_t status = FACE_DB_OK;
  if (index >= db->count) {
    status = FACE_DB_ERR_NOT_FOUND;
  } else {
    out_entry->id = db->slots[index].id;
    copy_name(out_entry->name, db->slots[index].name);
  }
  db_unlock(db);
  return status;
}

face_db_status_t face_db_find_best(face_db_t *db, const float *embedding,
                                   size_t embedding_len,
                                   face_db_match_t *out_match) {
  if (db == NULL || embedding == NULL || out_match == NULL ||
      embedding_len != db->embedding_len) {
    return FACE_DB_ERR_INVALID_ARG;
  }
  db_lock(db);
  face_db_status_t status = FACE_DB_OK;
  if (db->count == 0) {
    status = FACE_DB_ERR_EMPTY;
  } else if (!normalise(embedding, db->query, db->embedding_len)) {
    status = FACE_DB_ERR_INVALID_ARG;
  } else {
    size_t best_index = 0;
    float best_similarity = -2.0f;
    for (size_t i = 0; i < db->count; i++) {
      const float similarity =
          dot_product(db->query, embedding_at(db, i), db->embedding_len);
      if (similarity > best_similarity) {
        best_similarity = similarity;
        best_index = i;
      }
    }
    out_match->id = db->slots[best_index].id;
    out_match->similarity = best_similarity;
    copy_name(out_match->name, db->slots[best_index].name);
  }
  db_unlock(db);
  return status;
}

const char *face_db_status_name(face_db_status_t status) {
  switch (status) {
  case FACE_DB_OK:
    return "ok";
  case FACE_DB_ERR_INVALID_ARG:
    return "invalid argument";
  case FACE_DB_ERR_NO_MEM:
    return "out of memory";
  case FACE_DB_ERR_FULL:
    return "face store full";
  case FACE_DB_ERR_NOT_FOUND:
    return "face not found";
  case FACE_DB_ERR_EMPTY:
    return "no face enrolled";
  case FACE_DB_ERR_STORAGE:
    return "storage error";
  }
  return "unknown";
}

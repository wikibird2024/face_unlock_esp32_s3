/**
 * @file face_db.h
 * @brief Store of enrolled faces and the matching logic.
 *
 * Plain C with no ESP-IDF dependency, so it runs in host unit tests.
 * Persistence and locking are injected through face_db_storage_t and
 * face_db_lock_t. On the device, face_db_nvs.h provides both.
 *
 * Embeddings are L2-normalised when enrolled and when searched, so the
 * similarity is the cosine similarity in the range -1.0 .. 1.0.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Id 0 is never given to a face; it means "no face". */
#define FACE_DB_NO_ID 0u
#define FACE_DB_NAME_MAX_LEN 31u

typedef enum {
  FACE_DB_OK = 0,
  FACE_DB_ERR_INVALID_ARG,
  FACE_DB_ERR_NO_MEM,
  FACE_DB_ERR_FULL,
  FACE_DB_ERR_NOT_FOUND,
  FACE_DB_ERR_EMPTY,
  FACE_DB_ERR_STORAGE,
} face_db_status_t;

typedef struct face_db face_db_t;

/** One stored face as passed to and from the storage backend. */
typedef struct {
  uint16_t id;
  const char *name;
  const float *embedding;
  size_t embedding_len;
} face_db_record_t;

/** Called by storage->load_all() once per saved record. */
typedef face_db_status_t (*face_db_add_record_fn)(
    face_db_t *db, const face_db_record_t *record);

/** Persistence backend. Every function receives `context` first. */
typedef struct {
  void *context;
  face_db_status_t (*load_all)(void *context, face_db_t *db,
                               face_db_add_record_fn add_record);
  face_db_status_t (*save)(void *context, const face_db_record_t *record);
  face_db_status_t (*erase)(void *context, uint16_t id);
  face_db_status_t (*erase_all)(void *context);
} face_db_storage_t;

/** Optional mutex so several tasks can share one face_db_t. */
typedef struct {
  void *context;
  void (*lock)(void *context);
  void (*unlock)(void *context);
} face_db_lock_t;

typedef struct {
  size_t max_faces;
  size_t embedding_len;
  /** NULL: faces live in RAM only. */
  const face_db_storage_t *storage;
  /** NULL: no locking (single task use). */
  const face_db_lock_t *lock;
} face_db_config_t;

/** Public information about one enrolled face. */
typedef struct {
  uint16_t id;
  char name[FACE_DB_NAME_MAX_LEN + 1];
} face_db_entry_t;

/** Result of face_db_find_best(). */
typedef struct {
  uint16_t id;
  float similarity;
  char name[FACE_DB_NAME_MAX_LEN + 1];
} face_db_match_t;

/**
 * Create a face store and load the saved faces from storage.
 * Saved records whose embedding length differs from config->embedding_len
 * (for example after a model change) are skipped and counted; see
 * face_db_skipped_count().
 */
face_db_status_t face_db_create(const face_db_config_t *config,
                                face_db_t **out_db);
void face_db_destroy(face_db_t *db);

/**
 * Add a face. The same name may be enrolled several times (several photos of
 * one person improve matching). Names longer than FACE_DB_NAME_MAX_LEN are
 * cut.
 */
face_db_status_t face_db_enroll(face_db_t *db, const char *name,
                                const float *embedding, size_t embedding_len,
                                uint16_t *out_id);
face_db_status_t face_db_delete(face_db_t *db, uint16_t id);
face_db_status_t face_db_clear(face_db_t *db);

size_t face_db_count(face_db_t *db);
size_t face_db_capacity(const face_db_t *db);
size_t face_db_embedding_len(const face_db_t *db);
size_t face_db_skipped_count(const face_db_t *db);

/** Read entry number `index` (0 .. face_db_count() - 1). */
face_db_status_t face_db_get_entry(face_db_t *db, size_t index,
                                   face_db_entry_t *out_entry);

/**
 * Find the enrolled face most similar to `embedding`.
 * The caller decides whether the similarity is high enough.
 * @return FACE_DB_ERR_EMPTY when no face is enrolled.
 */
face_db_status_t face_db_find_best(face_db_t *db, const float *embedding,
                                   size_t embedding_len,
                                   face_db_match_t *out_match);

/**
 * Add a record without saving it to storage. Used by storage->load_all();
 * not meant for application code.
 */
face_db_status_t face_db_add_loaded_record(face_db_t *db,
                                           const face_db_record_t *record);

const char *face_db_status_name(face_db_status_t status);

#ifdef __cplusplus
}
#endif

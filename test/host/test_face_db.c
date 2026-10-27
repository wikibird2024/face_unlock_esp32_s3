#include "face_db.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

#define TEST_EMBEDDING_LEN 8u
#define TEST_MAX_FACES 4u
#define FAKE_STORAGE_SLOTS 8u

/* ---- Fake storage: keeps records in RAM, can be told to fail. ---------- */

typedef struct {
  bool is_used;
  uint16_t id;
  char name[FACE_DB_NAME_MAX_LEN + 1];
  size_t embedding_len;
  float embedding[TEST_EMBEDDING_LEN * 2];
} fake_record_t;

typedef struct {
  fake_record_t records[FAKE_STORAGE_SLOTS];
  bool should_fail_save;
  bool should_fail_erase;
  int save_count;
} fake_storage_t;

static face_db_status_t fake_load_all(void *context, face_db_t *db,
                                      face_db_add_record_fn add_record) {
  fake_storage_t *storage = context;
  for (size_t i = 0; i < FAKE_STORAGE_SLOTS; i++) {
    const fake_record_t *saved = &storage->records[i];
    if (!saved->is_used) {
      continue;
    }
    const face_db_record_t record = {.id = saved->id,
                                     .name = saved->name,
                                     .embedding = saved->embedding,
                                     .embedding_len = saved->embedding_len};
    face_db_status_t status = add_record(db, &record);
    if (status != FACE_DB_OK) {
      return status;
    }
  }
  return FACE_DB_OK;
}

static face_db_status_t fake_save(void *context,
                                  const face_db_record_t *record) {
  fake_storage_t *storage = context;
  if (storage->should_fail_save) {
    return FACE_DB_ERR_STORAGE;
  }
  for (size_t i = 0; i < FAKE_STORAGE_SLOTS; i++) {
    fake_record_t *slot = &storage->records[i];
    if (!slot->is_used) {
      slot->is_used = true;
      slot->id = record->id;
      strncpy(slot->name, record->name, FACE_DB_NAME_MAX_LEN);
      slot->embedding_len = record->embedding_len;
      memcpy(slot->embedding, record->embedding,
             record->embedding_len * sizeof(float));
      storage->save_count++;
      return FACE_DB_OK;
    }
  }
  return FACE_DB_ERR_STORAGE;
}

static face_db_status_t fake_erase(void *context, uint16_t id) {
  fake_storage_t *storage = context;
  if (storage->should_fail_erase) {
    return FACE_DB_ERR_STORAGE;
  }
  for (size_t i = 0; i < FAKE_STORAGE_SLOTS; i++) {
    if (storage->records[i].is_used && storage->records[i].id == id) {
      storage->records[i].is_used = false;
    }
  }
  return FACE_DB_OK;
}

static face_db_status_t fake_erase_all(void *context) {
  fake_storage_t *storage = context;
  memset(storage->records, 0, sizeof(storage->records));
  return FACE_DB_OK;
}

/* ---- Fake lock: counts calls and checks they are balanced. ------------- */

typedef struct {
  int depth;
  int lock_count;
} fake_lock_t;

static void fake_lock(void *context) {
  fake_lock_t *lock = context;
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, lock->depth, "lock taken twice");
  lock->depth++;
  lock->lock_count++;
}

static void fake_unlock(void *context) {
  fake_lock_t *lock = context;
  TEST_ASSERT_EQUAL_INT_MESSAGE(1, lock->depth, "unlock without lock");
  lock->depth--;
}

/* ---- Helpers ------------------------------------------------------------ */

static fake_storage_t storage_state;
static face_db_storage_t storage;
static face_db_t *db;

/* A unit vector along axis `axis` with a small tilt, times `scale`. */
static void make_embedding(float *out, size_t axis, float scale) {
  for (size_t i = 0; i < TEST_EMBEDDING_LEN; i++) {
    out[i] = 0.0f;
  }
  out[axis] = 1.0f * scale;
}

static face_db_t *create_db(void) {
  face_db_config_t config = {.max_faces = TEST_MAX_FACES,
                             .embedding_len = TEST_EMBEDDING_LEN,
                             .storage = &storage,
                             .lock = NULL};
  face_db_t *created = NULL;
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_create(&config, &created));
  TEST_ASSERT_NOT_NULL(created);
  return created;
}

void setUp(void) {
  memset(&storage_state, 0, sizeof(storage_state));
  storage = (face_db_storage_t){.context = &storage_state,
                                .load_all = fake_load_all,
                                .save = fake_save,
                                .erase = fake_erase,
                                .erase_all = fake_erase_all};
  db = create_db();
}

void tearDown(void) {
  face_db_destroy(db);
  db = NULL;
}

/* ---- Tests -------------------------------------------------------------- */

static void test_create_rejects_bad_config(void) {
  face_db_t *created = NULL;
  face_db_config_t config = {.max_faces = 0, .embedding_len = 8};
  TEST_ASSERT_EQUAL(FACE_DB_ERR_INVALID_ARG, face_db_create(&config, &created));
  config = (face_db_config_t){.max_faces = 2, .embedding_len = 0};
  TEST_ASSERT_EQUAL(FACE_DB_ERR_INVALID_ARG, face_db_create(&config, &created));
  TEST_ASSERT_EQUAL(FACE_DB_ERR_INVALID_ARG, face_db_create(NULL, &created));
  TEST_ASSERT_NULL(created);
}

static void test_find_best_on_empty_store(void) {
  float query[TEST_EMBEDDING_LEN];
  make_embedding(query, 0, 1.0f);
  face_db_match_t match;
  TEST_ASSERT_EQUAL(FACE_DB_ERR_EMPTY,
                    face_db_find_best(db, query, TEST_EMBEDDING_LEN, &match));
}

static void test_enroll_then_find_same_face(void) {
  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 2, 5.0f); /* not unit length on purpose */
  uint16_t id = 0;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_enroll(db, "alice", face, TEST_EMBEDDING_LEN, &id));
  TEST_ASSERT_EQUAL_UINT16(1, id);
  TEST_ASSERT_EQUAL_size_t(1, face_db_count(db));

  face_db_match_t match;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_find_best(db, face, TEST_EMBEDDING_LEN, &match));
  TEST_ASSERT_EQUAL_UINT16(id, match.id);
  TEST_ASSERT_EQUAL_STRING("alice", match.name);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, match.similarity);
}

static void test_find_best_picks_most_similar(void) {
  float alice[TEST_EMBEDDING_LEN];
  float bob[TEST_EMBEDDING_LEN];
  make_embedding(alice, 0, 1.0f);
  make_embedding(bob, 1, 1.0f);
  uint16_t alice_id = 0;
  uint16_t bob_id = 0;
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_enroll(db, "alice", alice,
                                               TEST_EMBEDDING_LEN, &alice_id));
  TEST_ASSERT_EQUAL(
      FACE_DB_OK, face_db_enroll(db, "bob", bob, TEST_EMBEDDING_LEN, &bob_id));

  /* Mostly bob, a little alice: cos = 0.8 to bob, 0.6 to alice. */
  float query[TEST_EMBEDDING_LEN] = {0};
  query[0] = 0.6f;
  query[1] = 0.8f;
  face_db_match_t match;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_find_best(db, query, TEST_EMBEDDING_LEN, &match));
  TEST_ASSERT_EQUAL_UINT16(bob_id, match.id);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.8f, match.similarity);
}

static void test_orthogonal_face_has_zero_similarity(void) {
  float alice[TEST_EMBEDDING_LEN];
  float stranger[TEST_EMBEDDING_LEN];
  make_embedding(alice, 0, 1.0f);
  make_embedding(stranger, 5, 1.0f);
  TEST_ASSERT_EQUAL(
      FACE_DB_OK, face_db_enroll(db, "alice", alice, TEST_EMBEDDING_LEN, NULL));
  face_db_match_t match;
  TEST_ASSERT_EQUAL(
      FACE_DB_OK, face_db_find_best(db, stranger, TEST_EMBEDDING_LEN, &match));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, match.similarity);
}

static void test_enroll_rejects_wrong_length_and_zero_vector(void) {
  float face[TEST_EMBEDDING_LEN] = {0};
  TEST_ASSERT_EQUAL(FACE_DB_ERR_INVALID_ARG,
                    face_db_enroll(db, "zero", face, TEST_EMBEDDING_LEN, NULL));
  face[0] = 1.0f;
  TEST_ASSERT_EQUAL(
      FACE_DB_ERR_INVALID_ARG,
      face_db_enroll(db, "short", face, TEST_EMBEDDING_LEN - 1, NULL));
  TEST_ASSERT_EQUAL(FACE_DB_ERR_INVALID_ARG,
                    face_db_enroll(db, "", face, TEST_EMBEDDING_LEN, NULL));
  TEST_ASSERT_EQUAL_size_t(0, face_db_count(db));
  TEST_ASSERT_EQUAL_INT(0, storage_state.save_count);
}

static void test_store_full(void) {
  float face[TEST_EMBEDDING_LEN];
  for (size_t i = 0; i < TEST_MAX_FACES; i++) {
    make_embedding(face, i, 1.0f);
    TEST_ASSERT_EQUAL(
        FACE_DB_OK, face_db_enroll(db, "user", face, TEST_EMBEDDING_LEN, NULL));
  }
  TEST_ASSERT_EQUAL(FACE_DB_ERR_FULL, face_db_enroll(db, "extra", face,
                                                     TEST_EMBEDDING_LEN, NULL));
  TEST_ASSERT_EQUAL_size_t(TEST_MAX_FACES, face_db_count(db));
}

static void test_long_name_is_cut(void) {
  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  char long_name[64];
  memset(long_name, 'x', sizeof(long_name) - 1);
  long_name[sizeof(long_name) - 1] = '\0';
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_enroll(db, long_name, face,
                                               TEST_EMBEDDING_LEN, NULL));
  face_db_entry_t entry;
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_get_entry(db, 0, &entry));
  TEST_ASSERT_EQUAL_size_t(FACE_DB_NAME_MAX_LEN, strlen(entry.name));
}

static void test_delete_keeps_other_faces(void) {
  float face[TEST_EMBEDDING_LEN];
  uint16_t ids[3];
  const char *names[3] = {"a", "b", "c"};
  for (size_t i = 0; i < 3; i++) {
    make_embedding(face, i, 1.0f);
    TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_enroll(db, names[i], face,
                                                 TEST_EMBEDDING_LEN, &ids[i]));
  }
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_delete(db, ids[0]));
  TEST_ASSERT_EQUAL(FACE_DB_ERR_NOT_FOUND, face_db_delete(db, ids[0]));
  TEST_ASSERT_EQUAL_size_t(2, face_db_count(db));

  /* "c" moved into the freed slot; its embedding must have moved too. */
  make_embedding(face, 2, 1.0f);
  face_db_match_t match;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_find_best(db, face, TEST_EMBEDDING_LEN, &match));
  TEST_ASSERT_EQUAL_UINT16(ids[2], match.id);
  TEST_ASSERT_EQUAL_STRING("c", match.name);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, match.similarity);
}

static void test_ids_are_not_reused_after_delete(void) {
  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  uint16_t first = 0;
  uint16_t second = 0;
  uint16_t third = 0;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_enroll(db, "a", face, TEST_EMBEDDING_LEN, &first));
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_enroll(db, "b", face, TEST_EMBEDDING_LEN, &second));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_delete(db, first));
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_enroll(db, "c", face, TEST_EMBEDDING_LEN, &third));
  TEST_ASSERT_TRUE(third > second);
}

static void test_faces_survive_restart(void) {
  float alice[TEST_EMBEDDING_LEN];
  make_embedding(alice, 3, 2.0f);
  uint16_t id = 0;
  TEST_ASSERT_EQUAL(
      FACE_DB_OK, face_db_enroll(db, "alice", alice, TEST_EMBEDDING_LEN, &id));

  face_db_destroy(db);
  db = create_db();

  TEST_ASSERT_EQUAL_size_t(1, face_db_count(db));
  face_db_match_t match;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_find_best(db, alice, TEST_EMBEDDING_LEN, &match));
  TEST_ASSERT_EQUAL_UINT16(id, match.id);
  TEST_ASSERT_EQUAL_STRING("alice", match.name);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, match.similarity);
}

static void test_records_from_other_model_are_skipped(void) {
  fake_record_t *stale = &storage_state.records[0];
  stale->is_used = true;
  stale->id = 7;
  strcpy(stale->name, "old model");
  stale->embedding_len = TEST_EMBEDDING_LEN * 2;
  stale->embedding[0] = 1.0f;

  face_db_destroy(db);
  db = create_db();

  TEST_ASSERT_EQUAL_size_t(0, face_db_count(db));
  TEST_ASSERT_EQUAL_size_t(1, face_db_skipped_count(db));
}

/* Limit lowered in menuconfig below the number of saved faces: the store
 * must still open, with the extra faces skipped. */
static void test_more_saved_faces_than_capacity_are_skipped(void) {
  const size_t saved_count = TEST_MAX_FACES + 2;
  for (size_t i = 0; i < saved_count; i++) {
    fake_record_t *saved = &storage_state.records[i];
    saved->is_used = true;
    saved->id = (uint16_t)(i + 1);
    strcpy(saved->name, "saved");
    saved->embedding_len = TEST_EMBEDDING_LEN;
    make_embedding(saved->embedding, i % TEST_EMBEDDING_LEN, 1.0f);
  }

  face_db_destroy(db);
  db = create_db();

  TEST_ASSERT_EQUAL_size_t(TEST_MAX_FACES, face_db_count(db));
  TEST_ASSERT_EQUAL_size_t(2, face_db_skipped_count(db));
}

/* A new face must not take the id of a skipped face: the save would write
 * over that face's record in storage. */
static void test_new_face_keeps_skipped_faces_in_storage(void) {
  const size_t saved_count = TEST_MAX_FACES + 2;
  for (size_t i = 0; i < saved_count; i++) {
    fake_record_t *saved = &storage_state.records[i];
    saved->is_used = true;
    saved->id = (uint16_t)(i + 1);
    strcpy(saved->name, "saved");
    saved->embedding_len = TEST_EMBEDDING_LEN;
    make_embedding(saved->embedding, i % TEST_EMBEDDING_LEN, 1.0f);
  }
  face_db_destroy(db);
  db = create_db();
  face_db_entry_t first_loaded;
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_get_entry(db, 0, &first_loaded));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_delete(db, first_loaded.id));

  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  uint16_t new_id = 0;
  TEST_ASSERT_EQUAL(
      FACE_DB_OK, face_db_enroll(db, "new", face, TEST_EMBEDDING_LEN, &new_id));

  TEST_ASSERT_EQUAL_UINT16(saved_count + 1, new_id);
  size_t untouched_count = 0;
  for (size_t i = 0; i < FAKE_STORAGE_SLOTS; i++) {
    const fake_record_t *record = &storage_state.records[i];
    if (record->is_used && strcmp(record->name, "saved") == 0) {
      untouched_count++;
    }
  }
  TEST_ASSERT_EQUAL_size_t(saved_count - 1, untouched_count);
}

static void test_failed_save_leaves_store_unchanged(void) {
  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  storage_state.should_fail_save = true;
  TEST_ASSERT_EQUAL(
      FACE_DB_ERR_STORAGE,
      face_db_enroll(db, "alice", face, TEST_EMBEDDING_LEN, NULL));
  TEST_ASSERT_EQUAL_size_t(0, face_db_count(db));
}

static void test_failed_erase_keeps_face(void) {
  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  uint16_t id = 0;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_enroll(db, "alice", face, TEST_EMBEDDING_LEN, &id));
  storage_state.should_fail_erase = true;
  TEST_ASSERT_EQUAL(FACE_DB_ERR_STORAGE, face_db_delete(db, id));
  TEST_ASSERT_EQUAL_size_t(1, face_db_count(db));
}

static void test_clear_removes_everything(void) {
  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_enroll(db, "a", face, TEST_EMBEDDING_LEN, NULL));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_clear(db));
  TEST_ASSERT_EQUAL_size_t(0, face_db_count(db));

  face_db_destroy(db);
  db = create_db();
  TEST_ASSERT_EQUAL_size_t(0, face_db_count(db));
}

static void test_lock_is_used_and_balanced(void) {
  fake_lock_t lock_state = {0};
  const face_db_lock_t lock = {
      .context = &lock_state, .lock = fake_lock, .unlock = fake_unlock};
  const face_db_config_t config = {.max_faces = TEST_MAX_FACES,
                                   .embedding_len = TEST_EMBEDDING_LEN,
                                   .storage = NULL,
                                   .lock = &lock};
  face_db_t *locked_db = NULL;
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_create(&config, &locked_db));

  float face[TEST_EMBEDDING_LEN];
  make_embedding(face, 0, 1.0f);
  face_db_match_t match;
  face_db_entry_t entry;
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_enroll(locked_db, "a", face,
                                               TEST_EMBEDDING_LEN, NULL));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_find_best(locked_db, face,
                                                  TEST_EMBEDDING_LEN, &match));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_get_entry(locked_db, 0, &entry));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_delete(locked_db, entry.id));

  TEST_ASSERT_EQUAL_INT(0, lock_state.depth);
  TEST_ASSERT_EQUAL_INT(4, lock_state.lock_count);
  face_db_destroy(locked_db);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_rejects_bad_config);
  RUN_TEST(test_find_best_on_empty_store);
  RUN_TEST(test_enroll_then_find_same_face);
  RUN_TEST(test_find_best_picks_most_similar);
  RUN_TEST(test_orthogonal_face_has_zero_similarity);
  RUN_TEST(test_enroll_rejects_wrong_length_and_zero_vector);
  RUN_TEST(test_store_full);
  RUN_TEST(test_long_name_is_cut);
  RUN_TEST(test_delete_keeps_other_faces);
  RUN_TEST(test_ids_are_not_reused_after_delete);
  RUN_TEST(test_faces_survive_restart);
  RUN_TEST(test_records_from_other_model_are_skipped);
  RUN_TEST(test_more_saved_faces_than_capacity_are_skipped);
  RUN_TEST(test_new_face_keeps_skipped_faces_in_storage);
  RUN_TEST(test_failed_save_leaves_store_unchanged);
  RUN_TEST(test_failed_erase_keeps_face);
  RUN_TEST(test_clear_removes_everything);
  RUN_TEST(test_lock_is_used_and_balanced);
  return UNITY_END();
}

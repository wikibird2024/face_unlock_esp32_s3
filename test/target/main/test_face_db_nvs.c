#include <string.h>

#include "face_db_nvs.h"
#include "unity.h"

#define EMBEDDING_LEN 512

static face_db_t *open_db(void) {
  /* face_db_nvs_open() may run only once per boot; share the store. */
  static face_db_t *db;
  if (db == NULL) {
    TEST_ASSERT_EQUAL(ESP_OK, face_db_nvs_open(EMBEDDING_LEN, &db));
  }
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_clear(db));
  return db;
}

static void fill_embedding(float *embedding, size_t axis) {
  memset(embedding, 0, EMBEDDING_LEN * sizeof(float));
  embedding[axis] = 1.0f;
}

TEST_CASE("face_db_nvs enroll, find and delete on flash", "[face_db]") {
  face_db_t *db = open_db();
  static float embedding[EMBEDDING_LEN];
  fill_embedding(embedding, 10);

  uint16_t id = 0;
  TEST_ASSERT_EQUAL(
      FACE_DB_OK, face_db_enroll(db, "tester", embedding, EMBEDDING_LEN, &id));
  TEST_ASSERT_EQUAL_size_t(1, face_db_count(db));

  face_db_match_t match;
  TEST_ASSERT_EQUAL(FACE_DB_OK,
                    face_db_find_best(db, embedding, EMBEDDING_LEN, &match));
  TEST_ASSERT_EQUAL_UINT16(id, match.id);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, match.similarity);

  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_delete(db, id));
  TEST_ASSERT_EQUAL_size_t(0, face_db_count(db));
}

TEST_CASE("face_db_nvs fills to capacity", "[face_db]") {
  face_db_t *db = open_db();
  static float embedding[EMBEDDING_LEN];
  for (size_t i = 0; i < face_db_capacity(db); i++) {
    fill_embedding(embedding, i);
    TEST_ASSERT_EQUAL(
        FACE_DB_OK, face_db_enroll(db, "user", embedding, EMBEDDING_LEN, NULL));
  }
  TEST_ASSERT_EQUAL(FACE_DB_ERR_FULL, face_db_enroll(db, "extra", embedding,
                                                     EMBEDDING_LEN, NULL));
  TEST_ASSERT_EQUAL(FACE_DB_OK, face_db_clear(db));
}

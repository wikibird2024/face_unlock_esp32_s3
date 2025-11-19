/**
 * @file test_data_handler.c
 * @brief Unit tests for data_handler component using Unity (ESP-IDF)
 */

#include "data_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include <string.h>

static const char *TAG = "test_data_handler";

/* -------------------------------------------------------------------------- */
/*                               UNITY HOOKS */
/* -------------------------------------------------------------------------- */

void setUp(void) {
  // Reset queues before each test
  TEST_ASSERT_EQUAL(ESP_OK, data_handler_init());
}

void tearDown(void) {
  // Nothing to cleanup; all memory is freed inside test cases
}

/* -------------------------------------------------------------------------- */
/*                               MOCK HELPERS */
/* -------------------------------------------------------------------------- */

static uint8_t *generate_mock_frame(size_t size) {
  uint8_t *buf = malloc(size);
  TEST_ASSERT_NOT_NULL(buf);

  for (size_t i = 0; i < size; i++) {
    buf[i] = i % 256;
  }
  return buf;
}

static float *generate_mock_embedding(size_t len) {
  float *emb = malloc(sizeof(float) * len);
  TEST_ASSERT_NOT_NULL(emb);

  for (size_t i = 0; i < len; i++) {
    emb[i] = (float)i * 0.1f;
  }
  return emb;
}

/* -------------------------------------------------------------------------- */
/*                           TEST CASE: PUSH/POP FRAME */
/* -------------------------------------------------------------------------- */

void test_push_pop_frame(void) {
  ESP_LOGI(TAG, "Running: test_push_pop_frame");

  size_t frame_size = 128;
  uint8_t *mock = generate_mock_frame(frame_size);

  uint64_t ts_in = 123456;

  TEST_ASSERT_EQUAL(ESP_OK, data_handler_push_frame(mock, frame_size, ts_in));

  camera_frame_t out = {0};
  TEST_ASSERT_EQUAL(ESP_OK, data_handler_pop_frame(&out, 100));

  TEST_ASSERT_EQUAL(ts_in, out.timestamp);
  TEST_ASSERT_EQUAL(frame_size, out.size);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(mock, out.data, frame_size);

  free(mock);
  free(out.data);
}

/* -------------------------------------------------------------------------- */
/*                       TEST CASE: PUSH/POP EMBEDDING */
/* -------------------------------------------------------------------------- */

void test_push_pop_embedding(void) {
  ESP_LOGI(TAG, "Running: test_push_pop_embedding");

  size_t len = 16;
  float *emb = generate_mock_embedding(len);

  TEST_ASSERT_EQUAL(ESP_OK, data_handler_push_embedding(emb, len));

  face_embedding_t out = {0};
  TEST_ASSERT_EQUAL(ESP_OK, data_handler_pop_embedding(&out, 100));

  TEST_ASSERT_EQUAL(len, out.length);
  TEST_ASSERT_EQUAL_FLOAT_ARRAY(emb, out.data, len);

  free(emb);
  free(out.data);
}

/* -------------------------------------------------------------------------- */
/*                     TEST CASE: QUEUE OVERFLOW / STRESS */
/* -------------------------------------------------------------------------- */

void test_queue_stress(void) {
  ESP_LOGI(TAG, "Running: test_queue_stress");

  const int limit = CONFIG_DATA_HANDLER_QUEUE_LENGTH;

  uint8_t *mock = generate_mock_frame(64);

  // Push until queue is full
  for (int i = 0; i < limit; i++) {
    TEST_ASSERT_EQUAL(ESP_OK, data_handler_push_frame(mock, 64, i));
  }

  // Next push must fail (timeout)
  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, data_handler_push_frame(mock, 64, 999));

  free(mock);
}

/* -------------------------------------------------------------------------- */
/*                       TEST CASE: MULTI-TASK CONCURRENCY */
/* -------------------------------------------------------------------------- */

static void camera_task(void *arg) {
  for (int i = 0; i < 20; i++) {
    uint8_t *frame = generate_mock_frame(32);
    data_handler_push_frame(frame, 32, i);
    free(frame);
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

static void ai_task(void *arg) {
  camera_frame_t frame;
  for (int i = 0; i < 20; i++) {
    if (data_handler_pop_frame(&frame, 200) == ESP_OK) {
      free(frame.data);
    }
  }
  vTaskDelete(NULL);
}

void test_multitask(void) {
  ESP_LOGI(TAG, "Running: test_multitask");

  xTaskCreate(camera_task, "cam_task", 4096, NULL, 5, NULL);
  xTaskCreate(ai_task, "ai_task", 4096, NULL, 5, NULL);

  // Allow tasks to complete
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  // If system survives without crash → PASS
  TEST_ASSERT_TRUE(true);
}

/* -------------------------------------------------------------------------- */
/*                                   MAIN */
/* -------------------------------------------------------------------------- */

void app_main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_push_pop_frame);
  RUN_TEST(test_push_pop_embedding);
  RUN_TEST(test_queue_stress);
  RUN_TEST(test_multitask);

  UNITY_END();
}

#include "data_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h" // Includes CONFIG_DATA_HANDLER_QUEUE_LENGTH
#include <stdlib.h>
#include <string.h>

static const char *TAG = "data_handler";

// Global Queue Handles
static QueueHandle_t frame_queue = NULL;
static QueueHandle_t embedding_queue = NULL;

// Define a safe default if Kconfig macro is somehow missing
#ifndef CONFIG_DATA_HANDLER_QUEUE_LENGTH
#define CONFIG_DATA_HANDLER_QUEUE_LENGTH 5
#endif

// --- Initialization --------------------------------------------------------

esp_err_t data_handler_init(void) {
  ESP_LOGI(TAG, "Starting data handler initialization...");

  // 1. Create Frame Queue
  frame_queue =
      xQueueCreate(CONFIG_DATA_HANDLER_QUEUE_LENGTH, sizeof(camera_frame_t));
  if (frame_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create frame queue. Out of memory.");
    return ESP_ERR_NO_MEM;
  }

  // 2. Create Embedding Queue
  embedding_queue =
      xQueueCreate(CONFIG_DATA_HANDLER_QUEUE_LENGTH, sizeof(face_embedding_t));
  if (embedding_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create embedding queue. Out of memory.");
    vQueueDelete(frame_queue); // Cleanup the first queue
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Data handler initialized successfully. Queue length: %d",
           CONFIG_DATA_HANDLER_QUEUE_LENGTH);
  return ESP_OK;
}

// --- Frame Handling --------------------------------------------------------

esp_err_t data_handler_push_frame(uint8_t *frame, size_t len,
                                  uint32_t timestamp) {
  if (frame_queue == NULL || frame == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  camera_frame_t item;
  // Dynamic allocation required for variable size data
  item.frame = (uint8_t *)malloc(len);

  if (item.frame == NULL) {
    ESP_LOGE(TAG, "Frame push failed: Out of memory (size: %u)", len);
    return ESP_ERR_NO_MEM;
  }

  memcpy(item.frame, frame, len);
  item.len = len;
  item.timestamp = timestamp;

  // Attempt to send immediately (non-blocking, wait time 0)
  if (xQueueSend(frame_queue, &item, 0) != pdTRUE) {
    // Send failed (queue full): MUST free the allocated memory!
    free(item.frame);
    return ESP_ERR_NO_BUF; // Use NO_BUF for queue full condition
  }

  return ESP_OK;
}

esp_err_t data_handler_pop_frame(camera_frame_t *frame, uint32_t timeout) {
  if (frame_queue == NULL || frame == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  TickType_t ticks_to_wait = pdMS_TO_TICKS(timeout);

  // xQueueReceive will block up to ticks_to_wait
  if (xQueueReceive(frame_queue, frame, ticks_to_wait) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  // Success. The caller is responsible for freeing frame->frame
  return ESP_OK;
}

void data_handler_free_frame(camera_frame_t *frame) {
  if (frame != NULL && frame->frame != NULL) {
    free(frame->frame);
    frame->frame = NULL; // Set NULL to prevent double free risk
  } else {
    ESP_LOGW(TAG, "Attempted to free NULL or invalid frame pointer.");
  }
}

// --- Embedding Handling ----------------------------------------------------

esp_err_t data_handler_push_embedding(float *embedding, size_t len,
                                      uint32_t timestamp) {
  if (embedding_queue == NULL || embedding == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  size_t required_size = len * sizeof(float);
  face_embedding_t item;
  item.embedding = (float *)malloc(required_size);

  if (item.embedding == NULL) {
    ESP_LOGE(TAG, "Embedding push failed: Out of memory (size: %u)",
             required_size);
    return ESP_ERR_NO_MEM;
  }

  memcpy(item.embedding, embedding, required_size);
  item.len = len;
  item.timestamp = timestamp;

  // Attempt to send immediately (non-blocking, wait time 0)
  if (xQueueSend(embedding_queue, &item, 0) != pdTRUE) {
    // Send failed (queue full): MUST free the allocated memory!
    free(item.embedding);
    return ESP_ERR_NO_BUF;
  }

  return ESP_OK;
}

esp_err_t data_handler_pop_embedding(face_embedding_t *embedding,
                                     uint32_t timeout) {
  if (embedding_queue == NULL || embedding == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  TickType_t ticks_to_wait = pdMS_TO_TICKS(timeout);

  if (xQueueReceive(embedding_queue, embedding, ticks_to_wait) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  // Success. The caller is responsible for freeing embedding->embedding
  return ESP_OK;
}

void data_handler_free_embedding(face_embedding_t *embedding) {
  if (embedding != NULL && embedding->embedding != NULL) {
    free(embedding->embedding);
    embedding->embedding = NULL; // Set NULL to prevent double free risk
  } else {
    ESP_LOGW(TAG, "Attempted to free NULL or invalid embedding pointer.");
  }
}

#ifndef DATA_HANDLE_H
#define DATA_HANDLE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdin.h>

#ifdef __cplusplus
extern "C" {
#endif

// -- Data Structure

/**
 * @brief Structure
 * @note the frame
 * Must be freed
 */

typedef struct {
  uint8_t *frame;
  size_t len;
  uint32_t timestamp;
} camera_frame_t;

typedef struct {
  float *embedding;
  size_t len;
  uint32_t timestamp;
} face_embedding_t;

esp_err_t data_handler_init(void);

esp_err_t data_handler_push_frame(uint8_t *frame, size_t len,
                                  uint8_t timestamp);

esp_err_t data_handler_pop_frame(camera_frame_t *frame);

esp_err_t data_handler_push_embedding(float *embedding, size_t len,
                                      uint32_t timestamp);

esp_err_t data_handler_pop_embedding(float *embedding, size_t len,
                                     uint32_t timestamp);

#ifdef __cplusplus
}
#endif
#endif /* DATA_HANDLE_H */

/**
 * @file camera_driver.h
 * @brief OV5640 capture through esp32-camera.
 *
 * Frames are RGB565, big-endian (high byte first), which is what the camera
 * sends and what ESP-DL reads as DL_IMAGE_PIX_TYPE_RGB565BE. The buffer
 * belongs to the driver: return it with camera_driver_release_frame()
 * quickly, or capture stalls.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const uint8_t *data;
  size_t length_bytes;
  uint16_t width;
  uint16_t height;
  int64_t timestamp_us;
  /* Driver's own frame buffer; do not touch. */
  void *driver_frame;
} camera_frame_t;

esp_err_t camera_driver_init(void);

/**
 * Take the newest frame. Blocks until one is ready.
 * @return ESP_ERR_INVALID_STATE if not initialised, ESP_FAIL on capture error.
 */
esp_err_t camera_driver_get_frame(camera_frame_t *out_frame);

void camera_driver_release_frame(camera_frame_t *frame);

#ifdef __cplusplus
}
#endif

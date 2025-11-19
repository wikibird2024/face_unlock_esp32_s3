
#ifndef CAMERA_DRIVER_H
#define CAMERA_DRIVER_H

#include "esp_camera.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lightweight wrapper around camera frame buffer.
 * Users should call camera_driver_get_frame() and
 * camera_driver_release_frame().
 */
typedef struct {
  camera_fb_t *fb;
  int width;
  int height;
} camera_frame_t;

/**
 * Initialize the OV5640 camera.
 * Must be called before capturing frames.
 */
void camera_driver_init(void);

/**
 * Capture a single frame.
 * Returns a pointer to camera_frame_t or NULL if error.
 */
camera_frame_t *camera_driver_get_frame(void);

/**
 * Release a previously captured frame.
 */
void camera_driver_release_frame(camera_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_DRIVER_H

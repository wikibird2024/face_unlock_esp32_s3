/**
 * @file ai_face_recognition.h
 * @brief Face detection and face embedding with Espressif's ESP-DL models.
 *
 * Detection: human_face_detect (MSR+MNP by default).
 * Embedding: human_face_recognition feature model (MobileFaceNet by
 * default). Models are chosen in menuconfig under "models: ...".
 *
 * Inference only: no tasks, no storage. Not thread-safe; call from one task.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool has_face;
  /** Number of faces found; the biggest one is used. */
  uint16_t face_count;
  float detect_score;
  /** Biggest face: left, top, right, bottom in pixels. */
  int16_t box[4];
  /** Valid floats in the embedding buffer (0 when has_face is false). */
  size_t embedding_len;
  uint32_t inference_ms;
} ai_face_result_t;

/** Load both models. Takes a few hundred ms and several hundred KB PSRAM. */
esp_err_t ai_face_recognition_init(void);

/** Length of one embedding (512 for MobileFaceNet). 0 before init. */
size_t ai_face_recognition_embedding_len(void);

/**
 * Find the biggest face in an RGB565 big-endian image and compute its
 * embedding into `embedding` (at least ai_face_recognition_embedding_len()
 * floats).
 */
esp_err_t ai_face_recognition_run(const uint8_t *rgb565_image, uint16_t width,
                                  uint16_t height, ai_face_result_t *result,
                                  float *embedding, size_t embedding_capacity);

#ifdef __cplusplus
}
#endif

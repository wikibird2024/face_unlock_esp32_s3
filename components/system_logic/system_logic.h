/**
 * @file system_logic.h
 * @brief The recognition task: camera → face model → face store → unlock
 *        state machine → door, plus requests from the console and button.
 *
 * One task owns the state machine, so requests from other tasks are queued
 * and handled between frames. Results are posted as APP_EVENTS.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "face_db.h"
#include "unlock_state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unlock_state_t state;
  uint32_t frame_count;
  uint32_t face_frame_count;
  uint32_t last_inference_ms;
  /** Best match of the last frame with a face (0 if none enrolled). */
  uint16_t last_face_id;
  float last_similarity;
  float min_similarity;
} system_logic_status_t;

/** Start the recognition task. Camera, face model and door must be ready. */
esp_err_t system_logic_start(face_db_t *face_db);

/** Enroll the next face seen, under `name`. */
esp_err_t system_logic_request_enroll(const char *name);
esp_err_t system_logic_request_cancel_enroll(void);
esp_err_t system_logic_request_unlock(void);

esp_err_t system_logic_get_status(system_logic_status_t *out_status);

#ifdef __cplusplus
}
#endif

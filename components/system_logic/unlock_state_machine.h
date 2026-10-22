/**
 * @file unlock_state_machine.h
 * @brief Decides when to open the door, reject a face or enroll a face.
 *
 * Plain C with no ESP-IDF dependency (host unit tested). The caller feeds
 * one input per camera frame, per user request and per timer tick, and
 * carries out the returned action. Time is passed in as `now_ms` so tests
 * control it.
 *
 *   IDLE ──face──▶ CONFIRMING ──N frames, known───▶ UNLOCKED ──hold──▶ COOLDOWN ──▶ IDLE
 *                      │                              ▲
 *                      │ known + approval on          │ approve (same request id)
 *                      ├──────────▶ WAITING_APPROVAL ─┴─ deny / timeout ─▶ COOLDOWN
 *                      └─────── N frames, unknown ─▶ reject ─────────▶ COOLDOWN
 *   IDLE, CONFIRMING, COOLDOWN ──start enroll──▶ ENROLLING ──N face frames──▶ enroll ─▶ COOLDOWN
 *                                                    └──── timeout / cancel ────▶ IDLE
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  UNLOCK_STATE_IDLE = 0,
  UNLOCK_STATE_CONFIRMING,
  UNLOCK_STATE_UNLOCKED,
  UNLOCK_STATE_COOLDOWN,
  UNLOCK_STATE_ENROLLING,
  /** A known face was seen; the door opens only on a matching approve. */
  UNLOCK_STATE_WAITING_APPROVAL,
} unlock_state_t;

typedef struct {
  /** A face counts as known when its similarity is at least this. */
  float min_similarity;
  /** Same result needed on this many frames in a row (1 = act at once). */
  uint8_t confirm_frame_count;
  uint32_t unlock_hold_ms;
  /** Faces are ignored this long after an unlock, reject or enroll. */
  uint32_t cooldown_ms;
  uint32_t enroll_timeout_ms;
  /** 0 = a known face opens at once. Otherwise the face only asks for
   * approval (for example from a phone) and waits this long for it. */
  uint32_t approval_timeout_ms;
  /** Id of the first approval request (0 counts as 1). Pass a random number
   * on the device so an answer from before a restart cannot match. */
  uint32_t first_request_id;
} unlock_config_t;

typedef enum {
  UNLOCK_INPUT_TICK = 0,
  UNLOCK_INPUT_NO_FACE,
  /** A face was seen; see face_id and similarity. */
  UNLOCK_INPUT_FACE,
  UNLOCK_INPUT_MANUAL_UNLOCK,
  UNLOCK_INPUT_START_ENROLL,
  UNLOCK_INPUT_CANCEL_ENROLL,
  /** Answer to an approval request; see request_id. */
  UNLOCK_INPUT_APPROVE,
  UNLOCK_INPUT_DENY,
} unlock_input_type_t;

typedef struct {
  unlock_input_type_t type;
  /** Best matching enrolled face, or 0 when nobody is enrolled. */
  uint16_t face_id;
  float similarity;
  /** APPROVE / DENY: the request being answered. Other ids are ignored. */
  uint32_t request_id;
} unlock_input_t;

typedef enum {
  UNLOCK_ACTION_NONE = 0,
  UNLOCK_ACTION_OPEN_DOOR,
  UNLOCK_ACTION_CLOSE_DOOR,
  UNLOCK_ACTION_REJECT_FACE,
  /** Enroll the face in the frame that produced this action. */
  UNLOCK_ACTION_ENROLL_FACE,
  UNLOCK_ACTION_ENROLL_TIMEOUT,
  UNLOCK_ACTION_ENROLL_STARTED,
  UNLOCK_ACTION_ENROLL_CANCELLED,
  /** The request cannot be done now (for example enroll while unlocked). */
  UNLOCK_ACTION_BUSY,
  /** A known face waits for approval; see face_id and request_id. */
  UNLOCK_ACTION_ASK_APPROVAL,
  UNLOCK_ACTION_APPROVAL_DENIED,
  UNLOCK_ACTION_APPROVAL_TIMEOUT,
} unlock_action_type_t;

typedef struct {
  unlock_action_type_t type;
  /** OPEN_DOOR by face: the face id (0 for a manual unlock). Approval
   * actions: the face waiting for approval. */
  uint16_t face_id;
  float similarity;
  /** Approval actions: the request id (0 otherwise). */
  uint32_t request_id;
} unlock_action_t;

typedef struct {
  unlock_config_t config;
  unlock_state_t state;
  /* CONFIRMING: the face id being confirmed (0 = unknown face). */
  uint16_t candidate_id;
  float candidate_similarity;
  uint8_t matching_frame_count;
  /* End of the UNLOCKED, COOLDOWN, ENROLLING or WAITING_APPROVAL state. */
  uint32_t state_end_ms;
  /* WAITING_APPROVAL: the open request and the face it is for. */
  uint32_t request_id;
  uint16_t approval_face_id;
  float approval_similarity;
  uint32_t next_request_id;
} unlock_state_machine_t;

void unlock_sm_init(unlock_state_machine_t *machine,
                    const unlock_config_t *config);

unlock_action_t unlock_sm_handle(unlock_state_machine_t *machine,
                                 const unlock_input_t *input, uint32_t now_ms);

unlock_state_t unlock_sm_state(const unlock_state_machine_t *machine);

/** The request waiting for approval, or 0 when none is waiting. */
uint32_t unlock_sm_waiting_request_id(const unlock_state_machine_t *machine);

const char *unlock_state_name(unlock_state_t state);
const char *unlock_action_name(unlock_action_type_t action);

#ifdef __cplusplus
}
#endif

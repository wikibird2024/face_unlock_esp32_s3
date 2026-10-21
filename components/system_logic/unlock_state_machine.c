#include "unlock_state_machine.h"

#include <stddef.h>

/* True once `now_ms` has reached `end_ms`; safe across the 32-bit wrap. */
static bool time_reached(uint32_t now_ms, uint32_t end_ms) {
  return (int32_t)(now_ms - end_ms) >= 0;
}

static uint8_t frames_needed(const unlock_state_machine_t *machine) {
  return machine->config.confirm_frame_count == 0
             ? 1
             : machine->config.confirm_frame_count;
}

static unlock_action_t make_action(unlock_action_type_t type) {
  unlock_action_t action = {.type = type, .face_id = 0, .similarity = 0.0f};
  return action;
}

static void enter_state(unlock_state_machine_t *machine, unlock_state_t state,
                        uint32_t end_ms) {
  machine->state = state;
  machine->state_end_ms = end_ms;
  machine->candidate_id = 0;
  machine->candidate_similarity = 0.0f;
  machine->matching_frame_count = 0;
}

static unlock_action_t open_door(unlock_state_machine_t *machine,
                                 uint16_t face_id, float similarity,
                                 uint32_t now_ms) {
  enter_state(machine, UNLOCK_STATE_UNLOCKED,
              now_ms + machine->config.unlock_hold_ms);
  unlock_action_t action = make_action(UNLOCK_ACTION_OPEN_DOOR);
  action.face_id = face_id;
  action.similarity = similarity;
  return action;
}

static unlock_action_t handle_tick(unlock_state_machine_t *machine,
                                   uint32_t now_ms) {
  if (!time_reached(now_ms, machine->state_end_ms)) {
    return make_action(UNLOCK_ACTION_NONE);
  }
  switch (machine->state) {
  case UNLOCK_STATE_UNLOCKED:
    enter_state(machine, UNLOCK_STATE_COOLDOWN,
                now_ms + machine->config.cooldown_ms);
    return make_action(UNLOCK_ACTION_CLOSE_DOOR);
  case UNLOCK_STATE_COOLDOWN:
    enter_state(machine, UNLOCK_STATE_IDLE, now_ms);
    return make_action(UNLOCK_ACTION_NONE);
  case UNLOCK_STATE_ENROLLING:
    enter_state(machine, UNLOCK_STATE_IDLE, now_ms);
    return make_action(UNLOCK_ACTION_ENROLL_TIMEOUT);
  default:
    return make_action(UNLOCK_ACTION_NONE);
  }
}

static unlock_action_t handle_face(unlock_state_machine_t *machine,
                                   const unlock_input_t *input,
                                   uint32_t now_ms) {
  if (machine->state == UNLOCK_STATE_ENROLLING) {
    machine->matching_frame_count++;
    if (machine->matching_frame_count < frames_needed(machine)) {
      return make_action(UNLOCK_ACTION_NONE);
    }
    enter_state(machine, UNLOCK_STATE_COOLDOWN,
                now_ms + machine->config.cooldown_ms);
    return make_action(UNLOCK_ACTION_ENROLL_FACE);
  }
  if (machine->state != UNLOCK_STATE_IDLE &&
      machine->state != UNLOCK_STATE_CONFIRMING) {
    return make_action(UNLOCK_ACTION_NONE);
  }

  const bool is_known = input->face_id != 0 &&
                        input->similarity >= machine->config.min_similarity;
  const uint16_t candidate_id = is_known ? input->face_id : 0;

  if (machine->state == UNLOCK_STATE_CONFIRMING &&
      machine->candidate_id == candidate_id) {
    machine->matching_frame_count++;
  } else {
    machine->state = UNLOCK_STATE_CONFIRMING;
    machine->candidate_id = candidate_id;
    machine->matching_frame_count = 1;
  }
  machine->candidate_similarity = input->similarity;

  if (machine->matching_frame_count < frames_needed(machine)) {
    return make_action(UNLOCK_ACTION_NONE);
  }
  if (is_known) {
    return open_door(machine, candidate_id, input->similarity, now_ms);
  }
  const float similarity = input->similarity;
  enter_state(machine, UNLOCK_STATE_COOLDOWN,
              now_ms + machine->config.cooldown_ms);
  unlock_action_t action = make_action(UNLOCK_ACTION_REJECT_FACE);
  action.similarity = similarity;
  return action;
}

void unlock_sm_init(unlock_state_machine_t *machine,
                    const unlock_config_t *config) {
  machine->config = *config;
  enter_state(machine, UNLOCK_STATE_IDLE, 0);
}

unlock_action_t unlock_sm_handle(unlock_state_machine_t *machine,
                                 const unlock_input_t *input, uint32_t now_ms) {
  if (machine == NULL || input == NULL) {
    return make_action(UNLOCK_ACTION_NONE);
  }
  switch (input->type) {
  case UNLOCK_INPUT_TICK:
    return handle_tick(machine, now_ms);

  case UNLOCK_INPUT_NO_FACE:
    if (machine->state == UNLOCK_STATE_CONFIRMING) {
      enter_state(machine, UNLOCK_STATE_IDLE, now_ms);
    } else if (machine->state == UNLOCK_STATE_ENROLLING) {
      machine->matching_frame_count = 0;
    }
    return make_action(UNLOCK_ACTION_NONE);

  case UNLOCK_INPUT_FACE:
    return handle_face(machine, input, now_ms);

  case UNLOCK_INPUT_MANUAL_UNLOCK:
    return open_door(machine, 0, 0.0f, now_ms);

  case UNLOCK_INPUT_START_ENROLL:
    if (machine->state == UNLOCK_STATE_UNLOCKED) {
      return make_action(UNLOCK_ACTION_BUSY);
    }
    enter_state(machine, UNLOCK_STATE_ENROLLING,
                now_ms + machine->config.enroll_timeout_ms);
    return make_action(UNLOCK_ACTION_ENROLL_STARTED);

  case UNLOCK_INPUT_CANCEL_ENROLL:
    if (machine->state != UNLOCK_STATE_ENROLLING) {
      return make_action(UNLOCK_ACTION_NONE);
    }
    enter_state(machine, UNLOCK_STATE_IDLE, now_ms);
    return make_action(UNLOCK_ACTION_ENROLL_CANCELLED);
  }
  return make_action(UNLOCK_ACTION_NONE);
}

unlock_state_t unlock_sm_state(const unlock_state_machine_t *machine) {
  return machine->state;
}

const char *unlock_state_name(unlock_state_t state) {
  switch (state) {
  case UNLOCK_STATE_IDLE:
    return "idle";
  case UNLOCK_STATE_CONFIRMING:
    return "confirming";
  case UNLOCK_STATE_UNLOCKED:
    return "unlocked";
  case UNLOCK_STATE_COOLDOWN:
    return "cooldown";
  case UNLOCK_STATE_ENROLLING:
    return "enrolling";
  }
  return "unknown";
}

const char *unlock_action_name(unlock_action_type_t action) {
  switch (action) {
  case UNLOCK_ACTION_NONE:
    return "none";
  case UNLOCK_ACTION_OPEN_DOOR:
    return "open door";
  case UNLOCK_ACTION_CLOSE_DOOR:
    return "close door";
  case UNLOCK_ACTION_REJECT_FACE:
    return "reject face";
  case UNLOCK_ACTION_ENROLL_FACE:
    return "enroll face";
  case UNLOCK_ACTION_ENROLL_TIMEOUT:
    return "enroll timeout";
  case UNLOCK_ACTION_ENROLL_STARTED:
    return "enroll started";
  case UNLOCK_ACTION_ENROLL_CANCELLED:
    return "enroll cancelled";
  case UNLOCK_ACTION_BUSY:
    return "busy";
  }
  return "unknown";
}

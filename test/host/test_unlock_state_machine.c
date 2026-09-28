#include "unity.h"
#include "unlock_state_machine.h"

#define MIN_SIMILARITY 0.5f
#define CONFIRM_FRAMES 3u
#define HOLD_MS 3000u
#define COOLDOWN_MS 2000u
#define ENROLL_TIMEOUT_MS 10000u

static unlock_state_machine_t machine;
static uint32_t now_ms;

void setUp(void) {
  const unlock_config_t config = {.min_similarity = MIN_SIMILARITY,
                                  .confirm_frame_count = CONFIRM_FRAMES,
                                  .unlock_hold_ms = HOLD_MS,
                                  .cooldown_ms = COOLDOWN_MS,
                                  .enroll_timeout_ms = ENROLL_TIMEOUT_MS};
  unlock_sm_init(&machine, &config);
  now_ms = 1000;
}

void tearDown(void) {}

static unlock_action_t send(unlock_input_type_t type, uint16_t face_id,
                            float similarity) {
  const unlock_input_t input = {
      .type = type, .face_id = face_id, .similarity = similarity};
  return unlock_sm_handle(&machine, &input, now_ms);
}

static unlock_action_t face(uint16_t face_id, float similarity) {
  now_ms += 100;
  return send(UNLOCK_INPUT_FACE, face_id, similarity);
}

static unlock_action_t tick_after(uint32_t elapsed_ms) {
  now_ms += elapsed_ms;
  return send(UNLOCK_INPUT_TICK, 0, 0.0f);
}

static void unlock_with_known_face(void) {
  face(4, 0.9f);
  face(4, 0.9f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR, face(4, 0.9f).type);
}

/* ---- Recognition ------------------------------------------------------- */

static void test_known_face_opens_after_confirm_frames(void) {
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(4, 0.8f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_CONFIRMING, unlock_sm_state(&machine));
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(4, 0.8f).type);
  const unlock_action_t action = face(4, 0.85f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR, action.type);
  TEST_ASSERT_EQUAL_UINT16(4, action.face_id);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.85f, action.similarity);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_UNLOCKED, unlock_sm_state(&machine));
}

static void test_similarity_exactly_at_threshold_is_known(void) {
  face(4, MIN_SIMILARITY);
  face(4, MIN_SIMILARITY);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR, face(4, MIN_SIMILARITY).type);
}

static void test_low_similarity_is_rejected_not_opened(void) {
  face(4, 0.3f);
  face(4, 0.3f);
  const unlock_action_t action = face(4, 0.3f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_REJECT_FACE, action.type);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.3f, action.similarity);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_COOLDOWN, unlock_sm_state(&machine));
}

static void test_no_enrolled_faces_means_reject(void) {
  face(0, 0.0f);
  face(0, 0.0f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_REJECT_FACE, face(0, 0.0f).type);
}

static void test_changing_face_restarts_confirmation(void) {
  face(4, 0.9f);
  face(4, 0.9f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(5, 0.9f).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(5, 0.9f).type);
  const unlock_action_t action = face(5, 0.9f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR, action.type);
  TEST_ASSERT_EQUAL_UINT16(5, action.face_id);
}

static void test_known_then_unknown_restarts_confirmation(void) {
  face(4, 0.9f);
  face(4, 0.9f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(4, 0.2f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_CONFIRMING, unlock_sm_state(&machine));
}

static void test_face_lost_resets_confirmation(void) {
  face(4, 0.9f);
  face(4, 0.9f);
  send(UNLOCK_INPUT_NO_FACE, 0, 0.0f);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_IDLE, unlock_sm_state(&machine));
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(4, 0.9f).type);
}

static void test_confirm_count_zero_acts_on_first_frame(void) {
  const unlock_config_t config = {.min_similarity = MIN_SIMILARITY,
                                  .confirm_frame_count = 0,
                                  .unlock_hold_ms = HOLD_MS,
                                  .cooldown_ms = COOLDOWN_MS,
                                  .enroll_timeout_ms = ENROLL_TIMEOUT_MS};
  unlock_sm_init(&machine, &config);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR, face(4, 0.9f).type);
}

/* ---- Door timing --------------------------------------------------------- */

static void test_door_closes_after_hold_then_cooldown_then_idle(void) {
  unlock_with_known_face();
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, tick_after(HOLD_MS - 1).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_CLOSE_DOOR, tick_after(1).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_COOLDOWN, unlock_sm_state(&machine));
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, tick_after(COOLDOWN_MS - 1).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_COOLDOWN, unlock_sm_state(&machine));
  tick_after(1);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_IDLE, unlock_sm_state(&machine));
}

static void test_faces_ignored_while_unlocked_and_in_cooldown(void) {
  unlock_with_known_face();
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(4, 0.9f).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(0, 0.1f).type);
  tick_after(HOLD_MS);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(4, 0.9f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_COOLDOWN, unlock_sm_state(&machine));
}

static void test_timer_survives_clock_wrap(void) {
  now_ms = UINT32_MAX - 500;
  unlock_with_known_face(); /* unlocked just before the clock wraps */
  /* The end time has wrapped to a small number but "now" has not yet:
   * a plain `now >= end` compare would close the door here. */
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, tick_after(100).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, tick_after(HOLD_MS - 101).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_CLOSE_DOOR, tick_after(1).type);
}

static void test_manual_unlock_from_idle(void) {
  const unlock_action_t action = send(UNLOCK_INPUT_MANUAL_UNLOCK, 0, 0.0f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR, action.type);
  TEST_ASSERT_EQUAL_UINT16(0, action.face_id);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_UNLOCKED, unlock_sm_state(&machine));
}

static void test_manual_unlock_while_open_extends_hold(void) {
  unlock_with_known_face();
  tick_after(HOLD_MS - 100);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_OPEN_DOOR,
                    send(UNLOCK_INPUT_MANUAL_UNLOCK, 0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, tick_after(200).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_CLOSE_DOOR, tick_after(HOLD_MS).type);
}

/* ---- Enrollment ---------------------------------------------------------- */

static void test_enroll_after_confirm_frames(void) {
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_ENROLL_STARTED,
                    send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_ENROLLING, unlock_sm_state(&machine));
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_ENROLL_FACE, face(0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_COOLDOWN, unlock_sm_state(&machine));
}

static void test_enroll_does_not_open_door_for_known_face(void) {
  send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f);
  face(4, 0.99f);
  face(4, 0.99f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_ENROLL_FACE, face(4, 0.99f).type);
}

static void test_enroll_needs_frames_in_a_row(void) {
  send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f);
  face(0, 0.0f);
  face(0, 0.0f);
  send(UNLOCK_INPUT_NO_FACE, 0, 0.0f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, face(0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_ENROLLING, unlock_sm_state(&machine));
}

static void test_enroll_times_out(void) {
  send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE, tick_after(ENROLL_TIMEOUT_MS - 1).type);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_ENROLL_TIMEOUT, tick_after(1).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_IDLE, unlock_sm_state(&machine));
}

static void test_enroll_cancel(void) {
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_NONE,
                    send(UNLOCK_INPUT_CANCEL_ENROLL, 0, 0.0f).type);
  send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f);
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_ENROLL_CANCELLED,
                    send(UNLOCK_INPUT_CANCEL_ENROLL, 0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_IDLE, unlock_sm_state(&machine));
}

static void test_enroll_refused_while_unlocked(void) {
  unlock_with_known_face();
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_BUSY,
                    send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f).type);
  TEST_ASSERT_EQUAL(UNLOCK_STATE_UNLOCKED, unlock_sm_state(&machine));
}

static void test_enroll_allowed_during_cooldown(void) {
  face(0, 0.0f);
  face(0, 0.0f);
  face(0, 0.0f); /* reject -> cooldown */
  TEST_ASSERT_EQUAL(UNLOCK_ACTION_ENROLL_STARTED,
                    send(UNLOCK_INPUT_START_ENROLL, 0, 0.0f).type);
}

static void test_names_exist_for_all_values(void) {
  TEST_ASSERT_EQUAL_STRING("idle", unlock_state_name(UNLOCK_STATE_IDLE));
  TEST_ASSERT_EQUAL_STRING("enrolling",
                           unlock_state_name(UNLOCK_STATE_ENROLLING));
  TEST_ASSERT_EQUAL_STRING("open door",
                           unlock_action_name(UNLOCK_ACTION_OPEN_DOOR));
  TEST_ASSERT_EQUAL_STRING("busy", unlock_action_name(UNLOCK_ACTION_BUSY));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_known_face_opens_after_confirm_frames);
  RUN_TEST(test_similarity_exactly_at_threshold_is_known);
  RUN_TEST(test_low_similarity_is_rejected_not_opened);
  RUN_TEST(test_no_enrolled_faces_means_reject);
  RUN_TEST(test_changing_face_restarts_confirmation);
  RUN_TEST(test_known_then_unknown_restarts_confirmation);
  RUN_TEST(test_face_lost_resets_confirmation);
  RUN_TEST(test_confirm_count_zero_acts_on_first_frame);
  RUN_TEST(test_door_closes_after_hold_then_cooldown_then_idle);
  RUN_TEST(test_faces_ignored_while_unlocked_and_in_cooldown);
  RUN_TEST(test_timer_survives_clock_wrap);
  RUN_TEST(test_manual_unlock_from_idle);
  RUN_TEST(test_manual_unlock_while_open_extends_hold);
  RUN_TEST(test_enroll_after_confirm_frames);
  RUN_TEST(test_enroll_does_not_open_door_for_known_face);
  RUN_TEST(test_enroll_needs_frames_in_a_row);
  RUN_TEST(test_enroll_times_out);
  RUN_TEST(test_enroll_cancel);
  RUN_TEST(test_enroll_refused_while_unlocked);
  RUN_TEST(test_enroll_allowed_during_cooldown);
  RUN_TEST(test_names_exist_for_all_values);
  return UNITY_END();
}

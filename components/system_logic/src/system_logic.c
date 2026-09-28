#include "system_logic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai_face_recognition.h"
#include "app_events.h"
#include "camera_driver.h"
#include "door_control.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "system_logic";

#define REQUEST_QUEUE_LENGTH 4
#define CAMERA_RETRY_DELAY_MS 200
#define LOOP_YIELD_MS 10
#define ENROLL_BUTTON_GPIO CONFIG_SYSTEM_LOGIC_ENROLL_BUTTON_GPIO
/* ESP32-S3: 22-25 do not exist, 26-37 are flash and octal PSRAM. */
#if ENROLL_BUTTON_GPIO >= 22 && ENROLL_BUTTON_GPIO <= 37
#error "CONFIG_SYSTEM_LOGIC_ENROLL_BUTTON_GPIO is not usable on ESP32-S3"
#endif

typedef enum {
  REQUEST_ENROLL,
  REQUEST_CANCEL_ENROLL,
  REQUEST_UNLOCK,
} request_type_t;

typedef struct {
  request_type_t type;
  char name[FACE_DB_NAME_MAX_LEN + 1];
} request_t;

/* What the current frame showed; used when carrying out an action. */
typedef struct {
  bool has_embedding;
  face_db_match_t match;
} frame_result_t;

static QueueHandle_t s_requests;
static face_db_t *s_face_db;
static unlock_state_machine_t s_machine;
static float *s_embedding;
static char s_enroll_name[FACE_DB_NAME_MAX_LEN + 1];

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static system_logic_status_t s_status;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void post_event(app_event_id_t event_id, uint16_t face_id,
                       float similarity, const char *name) {
  app_event_face_t payload = {.face_id = face_id, .similarity = similarity};
  if (name != NULL) {
    strncpy(payload.name, name, APP_EVENT_NAME_MAX_LEN);
  }
  esp_err_t err =
      esp_event_post(APP_EVENTS, event_id, &payload, sizeof(payload), 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Event %s dropped: %s", app_event_name(event_id),
             esp_err_to_name(err));
  }
}

static void update_status(unlock_state_t state) {
  taskENTER_CRITICAL(&s_status_lock);
  s_status.state = state;
  taskEXIT_CRITICAL(&s_status_lock);
}

static void carry_out(const unlock_action_t *action,
                      const frame_result_t *frame) {
  switch (action->type) {
  case UNLOCK_ACTION_NONE:
    return;

  case UNLOCK_ACTION_OPEN_DOOR: {
    const char *name =
        action->face_id == FACE_DB_NO_ID ? "manual" : frame->match.name;
    ESP_LOGI(TAG, "OPEN for '%s' (id %u, similarity %.2f)", name,
             action->face_id, action->similarity);
    esp_err_t err = door_control_open(CONFIG_SYSTEM_LOGIC_UNLOCK_HOLD_MS);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Door open failed: %s", esp_err_to_name(err));
      return;
    }
    post_event(APP_EVENT_DOOR_OPENED, action->face_id, action->similarity,
               name);
    return;
  }

  case UNLOCK_ACTION_CLOSE_DOOR:
    door_control_close();
    post_event(APP_EVENT_DOOR_CLOSED, 0, 0.0f, NULL);
    return;

  case UNLOCK_ACTION_REJECT_FACE:
    ESP_LOGI(TAG, "REJECT unknown face (best similarity %.2f)",
             action->similarity);
    post_event(APP_EVENT_FACE_REJECTED, 0, action->similarity, NULL);
    return;

  case UNLOCK_ACTION_ENROLL_STARTED:
    ESP_LOGI(TAG, "Enrolling '%s': look at the camera", s_enroll_name);
    post_event(APP_EVENT_ENROLL_STARTED, 0, 0.0f, s_enroll_name);
    return;

  case UNLOCK_ACTION_ENROLL_FACE: {
    if (!frame->has_embedding) {
      /* The state machine only asks for this on a face frame. */
      ESP_LOGE(TAG, "Enroll without a face embedding");
      post_event(APP_EVENT_ENROLL_FAILED, 0, 0.0f, s_enroll_name);
      return;
    }
    uint16_t id = FACE_DB_NO_ID;
    face_db_status_t status =
        face_db_enroll(s_face_db, s_enroll_name, s_embedding,
                       face_db_embedding_len(s_face_db), &id);
    if (status != FACE_DB_OK) {
      ESP_LOGE(TAG, "Enroll '%s' failed: %s", s_enroll_name,
               face_db_status_name(status));
      post_event(APP_EVENT_ENROLL_FAILED, 0, 0.0f, s_enroll_name);
      return;
    }
    ESP_LOGI(TAG, "Enrolled '%s' as id %u (%u faces)", s_enroll_name, id,
             (unsigned)face_db_count(s_face_db));
    post_event(APP_EVENT_FACE_ENROLLED, id, 0.0f, s_enroll_name);
    return;
  }

  case UNLOCK_ACTION_ENROLL_TIMEOUT:
  case UNLOCK_ACTION_ENROLL_CANCELLED:
    ESP_LOGW(TAG, "Enroll '%s': %s", s_enroll_name,
             unlock_action_name(action->type));
    post_event(APP_EVENT_ENROLL_FAILED, 0, 0.0f, s_enroll_name);
    return;

  case UNLOCK_ACTION_BUSY:
    ESP_LOGW(TAG, "Request refused: door is open");
    post_event(APP_EVENT_ENROLL_FAILED, 0, 0.0f, s_enroll_name);
    return;
  }
}

static void handle_input(unlock_input_type_t type, const frame_result_t *frame,
                         uint16_t face_id, float similarity) {
  const unlock_input_t input = {
      .type = type, .face_id = face_id, .similarity = similarity};
  const unlock_state_t state_before = unlock_sm_state(&s_machine);
  const unlock_action_t action = unlock_sm_handle(&s_machine, &input, now_ms());
  if (state_before == UNLOCK_STATE_ENROLLING &&
      action.type == UNLOCK_ACTION_OPEN_DOOR) {
    /* A manual unlock ends a running enroll; tell listeners. */
    ESP_LOGW(TAG, "Enroll '%s' ended by manual unlock", s_enroll_name);
    post_event(APP_EVENT_ENROLL_FAILED, 0, 0.0f, s_enroll_name);
  }
  carry_out(&action, frame);
  update_status(unlock_sm_state(&s_machine));
}

static void handle_requests(void) {
  static const frame_result_t no_frame = {0};
  request_t request;
  while (xQueueReceive(s_requests, &request, 0) == pdTRUE) {
    switch (request.type) {
    case REQUEST_ENROLL:
      memcpy(s_enroll_name, request.name, sizeof(s_enroll_name));
      handle_input(UNLOCK_INPUT_START_ENROLL, &no_frame, 0, 0.0f);
      break;
    case REQUEST_CANCEL_ENROLL:
      handle_input(UNLOCK_INPUT_CANCEL_ENROLL, &no_frame, 0, 0.0f);
      break;
    case REQUEST_UNLOCK:
      handle_input(UNLOCK_INPUT_MANUAL_UNLOCK, &no_frame, 0, 0.0f);
      break;
    }
  }
}

/* Enroll button: active low, acted on once per press after two readings in
 * a row (debounce at the frame rate). */
static void poll_enroll_button(void) {
#if ENROLL_BUTTON_GPIO >= 0
  static int pressed_readings;
  static uint32_t button_enroll_count;
  if (gpio_get_level((gpio_num_t)ENROLL_BUTTON_GPIO) == 0) {
    pressed_readings++;
    if (pressed_readings == 2) {
      button_enroll_count++;
      char name[FACE_DB_NAME_MAX_LEN + 1];
      snprintf(name, sizeof(name), "user_%lu",
               (unsigned long)button_enroll_count);
      system_logic_request_enroll(name);
    }
  } else {
    pressed_readings = 0;
  }
#endif
}

static void process_frame(void) {
  camera_frame_t camera_frame;
  if (camera_driver_get_frame(&camera_frame) != ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(CAMERA_RETRY_DELAY_MS));
    return;
  }
  ai_face_result_t ai_result;
  const size_t embedding_len = face_db_embedding_len(s_face_db);
  esp_err_t err = ai_face_recognition_run(camera_frame.data, camera_frame.width,
                                          camera_frame.height, &ai_result,
                                          s_embedding, embedding_len);
  camera_driver_release_frame(&camera_frame);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Inference failed: %s", esp_err_to_name(err));
    return;
  }

  frame_result_t frame = {.has_embedding = ai_result.has_face};
  if (ai_result.has_face) {
    face_db_status_t status =
        face_db_find_best(s_face_db, s_embedding, embedding_len, &frame.match);
    if (status != FACE_DB_OK) {
      /* FACE_DB_ERR_EMPTY: nobody enrolled yet, so the face is unknown. */
      memset(&frame.match, 0, sizeof(frame.match));
    }
    ESP_LOGD(TAG, "Face score %.2f box [%d %d %d %d] best id %u sim %.3f",
             ai_result.detect_score, ai_result.box[0], ai_result.box[1],
             ai_result.box[2], ai_result.box[3], frame.match.id,
             frame.match.similarity);
  }

  taskENTER_CRITICAL(&s_status_lock);
  s_status.frame_count++;
  s_status.last_inference_ms = ai_result.inference_ms;
  if (ai_result.has_face) {
    s_status.face_frame_count++;
    s_status.last_face_id = frame.match.id;
    s_status.last_similarity = frame.match.similarity;
  }
  taskEXIT_CRITICAL(&s_status_lock);

  if (ai_result.has_face) {
    handle_input(UNLOCK_INPUT_FACE, &frame, frame.match.id,
                 frame.match.similarity);
  } else {
    handle_input(UNLOCK_INPUT_NO_FACE, &frame, 0, 0.0f);
  }
}

static void recognition_task(void *argument) {
  (void)argument;
  static const frame_result_t no_frame = {0};
  ESP_LOGI(TAG, "Recognition running (%u faces enrolled)",
           (unsigned)face_db_count(s_face_db));
  for (;;) {
    handle_requests();
    poll_enroll_button();
    process_frame();
    handle_input(UNLOCK_INPUT_TICK, &no_frame, 0, 0.0f);
    /* Let the idle task on this core run (task watchdog, heap cleanup)
     * even when a new frame is always ready. */
    vTaskDelay(pdMS_TO_TICKS(LOOP_YIELD_MS));
  }
}

static esp_err_t init_enroll_button(void) {
#if ENROLL_BUTTON_GPIO >= 0
  const gpio_config_t io_config = {
      .pin_bit_mask = 1ULL << ENROLL_BUTTON_GPIO,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&io_config), TAG, "button gpio failed");
  ESP_LOGI(TAG, "Enroll button on GPIO %d (hold to enroll)",
           ENROLL_BUTTON_GPIO);
#endif
  return ESP_OK;
}

esp_err_t system_logic_start(face_db_t *face_db) {
  ESP_RETURN_ON_FALSE(face_db != NULL, ESP_ERR_INVALID_ARG, TAG, "no face_db");
  ESP_RETURN_ON_FALSE(s_requests == NULL, ESP_ERR_INVALID_STATE, TAG,
                      "already started");
  ESP_RETURN_ON_FALSE(face_db_embedding_len(face_db) ==
                          ai_face_recognition_embedding_len(),
                      ESP_ERR_INVALID_STATE, TAG,
                      "face store and model embedding lengths differ");

  ESP_RETURN_ON_ERROR(init_enroll_button(), TAG, "enroll button");
  s_face_db = face_db;
  s_embedding = calloc(face_db_embedding_len(face_db), sizeof(float));
  ESP_RETURN_ON_FALSE(s_embedding != NULL, ESP_ERR_NO_MEM, TAG,
                      "no memory for embedding");

  const unlock_config_t config = {
      .min_similarity = CONFIG_SYSTEM_LOGIC_MIN_SIMILARITY_PERCENT / 100.0f,
      .confirm_frame_count = CONFIG_SYSTEM_LOGIC_CONFIRM_FRAMES,
      .unlock_hold_ms = CONFIG_SYSTEM_LOGIC_UNLOCK_HOLD_MS,
      .cooldown_ms = CONFIG_SYSTEM_LOGIC_COOLDOWN_MS,
      .enroll_timeout_ms = CONFIG_SYSTEM_LOGIC_ENROLL_TIMEOUT_MS,
  };
  unlock_sm_init(&s_machine, &config);
  s_status.min_similarity = config.min_similarity;
  update_status(unlock_sm_state(&s_machine));

  /* The queue is made last: requests are only accepted once the task that
   * reads them exists. */
  s_requests = xQueueCreate(REQUEST_QUEUE_LENGTH, sizeof(request_t));
  BaseType_t created = pdFAIL;
  if (s_requests != NULL) {
    created = xTaskCreatePinnedToCore(
        recognition_task, "face_logic", CONFIG_SYSTEM_LOGIC_TASK_STACK_SIZE,
        NULL, CONFIG_SYSTEM_LOGIC_TASK_PRIORITY, NULL, 1);
  }
  if (created != pdPASS) {
    if (s_requests != NULL) {
      vQueueDelete(s_requests);
      s_requests = NULL;
    }
    free(s_embedding);
    s_embedding = NULL;
    ESP_LOGE(TAG, "No memory for recognition task or queue");
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

static esp_err_t send_request(const request_t *request) {
  ESP_RETURN_ON_FALSE(s_requests != NULL, ESP_ERR_INVALID_STATE, TAG,
                      "not started");
  if (xQueueSend(s_requests, request, pdMS_TO_TICKS(100)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

esp_err_t system_logic_request_enroll(const char *name) {
  ESP_RETURN_ON_FALSE(name != NULL && name[0] != '\0', ESP_ERR_INVALID_ARG, TAG,
                      "empty name");
  request_t request = {.type = REQUEST_ENROLL};
  strncpy(request.name, name, FACE_DB_NAME_MAX_LEN);
  return send_request(&request);
}

esp_err_t system_logic_request_cancel_enroll(void) {
  const request_t request = {.type = REQUEST_CANCEL_ENROLL};
  return send_request(&request);
}

esp_err_t system_logic_request_unlock(void) {
  const request_t request = {.type = REQUEST_UNLOCK};
  return send_request(&request);
}

esp_err_t system_logic_get_status(system_logic_status_t *out_status) {
  ESP_RETURN_ON_FALSE(out_status != NULL, ESP_ERR_INVALID_ARG, TAG,
                      "no output");
  taskENTER_CRITICAL(&s_status_lock);
  *out_status = s_status;
  taskEXIT_CRITICAL(&s_status_lock);
  return ESP_OK;
}

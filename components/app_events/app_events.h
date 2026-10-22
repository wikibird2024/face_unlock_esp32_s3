/**
 * @file app_events.h
 * @brief Events posted on the default ESP-IDF event loop.
 *
 * system_logic posts them; any component (net_comm today, a display or
 * buzzer later) can listen with esp_event_handler_register(APP_EVENTS, ...)
 * without system_logic knowing about it.
 */
#pragma once

#include <stdint.h>

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(APP_EVENTS);

#define APP_EVENT_NAME_MAX_LEN 31

typedef enum {
  /** Payload app_event_face_t. face_id 0 means a manual unlock. */
  APP_EVENT_DOOR_OPENED,
  /** No payload. */
  APP_EVENT_DOOR_CLOSED,
  /** Payload app_event_face_t (similarity of the best match). */
  APP_EVENT_FACE_REJECTED,
  /** Payload app_event_face_t (name to be enrolled, face_id 0). */
  APP_EVENT_ENROLL_STARTED,
  /** Payload app_event_face_t (new face id and name). */
  APP_EVENT_FACE_ENROLLED,
  /** Payload app_event_face_t (name). Timeout, cancel or store error. */
  APP_EVENT_ENROLL_FAILED,
  /** Payload app_event_face_t (face, name, request_id). A known face waits
   * for system_logic_request_approve(request_id) or _deny(request_id). */
  APP_EVENT_APPROVAL_REQUESTED,
  /** Payload app_event_face_t (face, name, request_id). */
  APP_EVENT_APPROVAL_DENIED,
  /** Payload app_event_face_t (face, name, request_id). */
  APP_EVENT_APPROVAL_TIMEOUT,
} app_event_id_t;

typedef struct {
  uint16_t face_id;
  float similarity;
  char name[APP_EVENT_NAME_MAX_LEN + 1];
  /** Approval events: the request id; 0 for other events. */
  uint32_t request_id;
} app_event_face_t;

const char *app_event_name(int32_t event_id);

#ifdef __cplusplus
}
#endif

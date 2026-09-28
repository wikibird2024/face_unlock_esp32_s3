#include "app_events.h"

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

const char *app_event_name(int32_t event_id) {
  switch ((app_event_id_t)event_id) {
  case APP_EVENT_DOOR_OPENED:
    return "door_opened";
  case APP_EVENT_DOOR_CLOSED:
    return "door_closed";
  case APP_EVENT_FACE_REJECTED:
    return "face_rejected";
  case APP_EVENT_ENROLL_STARTED:
    return "enroll_started";
  case APP_EVENT_FACE_ENROLLED:
    return "face_enrolled";
  case APP_EVENT_ENROLL_FAILED:
    return "enroll_failed";
  }
  return "unknown";
}

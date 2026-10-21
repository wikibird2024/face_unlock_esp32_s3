#include "net_comm.h"

#include "app_events.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "net_comm";

#define TOPIC_PREFIX "face_unlock/"

static void on_app_event(void *handler_argument, esp_event_base_t base,
                         int32_t event_id, void *event_data) {
  (void)handler_argument;
  (void)base;
  const app_event_face_t *face = event_data;
  /* Placeholder for MQTT: log the topic and payload that would be sent. */
  if (face != NULL) {
    ESP_LOGI(TAG,
             "publish %s%s {\"id\":%u,\"name\":\"%s\",\"similarity\":%.3f}",
             TOPIC_PREFIX, app_event_name(event_id), face->face_id, face->name,
             face->similarity);
  } else {
    ESP_LOGI(TAG, "publish %s%s {}", TOPIC_PREFIX, app_event_name(event_id));
  }
}

esp_err_t net_comm_init(void) {
  ESP_RETURN_ON_ERROR(esp_event_handler_register(APP_EVENTS, ESP_EVENT_ANY_ID,
                                                 on_app_event, NULL),
                      TAG, "event register failed");
  ESP_LOGI(TAG, "Event reporting on (network not implemented yet)");
  return ESP_OK;
}

#include "net_comm.h"

#include <stdio.h>
#include <string.h>

#include "app_events.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "net_comm";

#define TOPIC_PREFIX "face_unlock/"

/* Set only from the default event loop task; read from other tasks. */
static volatile bool s_is_connected;
static char s_ip[NET_COMM_IP_MAX_LEN];

static void on_app_event(void *handler_argument, esp_event_base_t base,
                         int32_t event_id, void *event_data) {
  (void)handler_argument;
  (void)base;
  const app_event_face_t *face = event_data;
  /* Placeholder for MQTT: log the topic and payload that would be sent. */
  if (face != NULL) {
    ESP_LOGI(TAG,
             "publish %s%s {\"id\":%u,\"name\":\"%s\",\"similarity\":%.3f,"
             "\"request\":%lu}",
             TOPIC_PREFIX, app_event_name(event_id), face->face_id, face->name,
             face->similarity, (unsigned long)face->request_id);
  } else {
    ESP_LOGI(TAG, "publish %s%s {}", TOPIC_PREFIX, app_event_name(event_id));
  }
}

static void on_wifi_event(void *handler_argument, esp_event_base_t base,
                          int32_t event_id, void *event_data) {
  (void)handler_argument;
  if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    return;
  }
  if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    s_is_connected = false;
    s_ip[0] = '\0';
    ESP_LOGW(TAG, "Wi-Fi lost, reconnecting in %d ms",
             CONFIG_NET_COMM_WIFI_RETRY_DELAY_MS);
    /* This handler runs on the event loop task; a short block is fine. */
    vTaskDelay(pdMS_TO_TICKS(CONFIG_NET_COMM_WIFI_RETRY_DELAY_MS));
    esp_wifi_connect();
    return;
  }
  if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    const ip_event_got_ip_t *event = event_data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    s_is_connected = true;
    ESP_LOGI(TAG, "Wi-Fi connected, IP %s", s_ip);
  }
}

static esp_err_t start_wifi(void) {
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
  ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_sta() != NULL, ESP_FAIL,
                      TAG, "netif sta");

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "wifi init");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 on_wifi_event, NULL),
                      TAG, "wifi handler");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 on_wifi_event, NULL),
                      TAG, "ip handler");

  wifi_config_t wifi_config = {0};
  strncpy((char *)wifi_config.sta.ssid, CONFIG_NET_COMM_WIFI_SSID,
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, CONFIG_NET_COMM_WIFI_PASSWORD,
          sizeof(wifi_config.sta.password) - 1);
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG,
                      "wifi config");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
  ESP_LOGI(TAG, "Joining Wi-Fi '%s'", CONFIG_NET_COMM_WIFI_SSID);
  return ESP_OK;
}

esp_err_t net_comm_init(void) {
  ESP_RETURN_ON_ERROR(esp_event_handler_register(APP_EVENTS, ESP_EVENT_ANY_ID,
                                                 on_app_event, NULL),
                      TAG, "event register failed");
  if (CONFIG_NET_COMM_WIFI_SSID[0] == '\0') {
    ESP_LOGI(TAG, "Wi-Fi off (no SSID in menuconfig); events are only logged");
    return ESP_OK;
  }
  /* A Wi-Fi problem must not stop the door: report it and carry on. */
  esp_err_t err = start_wifi();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi not started: %s", esp_err_to_name(err));
  }
  return ESP_OK;
}

bool net_comm_is_connected(void) { return s_is_connected; }

void net_comm_get_ip(char *out_ip) {
  if (out_ip == NULL) {
    return;
  }
  if (s_is_connected) {
    memcpy(out_ip, s_ip, NET_COMM_IP_MAX_LEN);
  } else {
    out_ip[0] = '\0';
  }
}

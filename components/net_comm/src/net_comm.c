#include "net_comm.h"
#include "esp_log.h"

static const char *TAG = "net_comm";

void net_comm_init(void)
{
    ESP_LOGI(TAG, "Initialize WiFi + MQTT (optional)");
}

void net_comm_publish(const char *topic, const char *data)
{
    ESP_LOGI(TAG, "MQTT publish: [%s] -> %s", topic, data);
}

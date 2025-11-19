#include "system_logic.h"
#include "esp_log.h"

static const char *TAG = "system_logic";

esp_err_t system_logic_init(void)
{
    ESP_LOGI(TAG, "system_logic initialized");
    return ESP_OK;
}

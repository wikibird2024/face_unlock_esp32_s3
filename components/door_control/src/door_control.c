#include "door_control.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "door_control";
#define LOCK_GPIO GPIO_NUM_2
#define UNLOCK_TIME_MS 3000

void door_control_init(void)
{
    ESP_LOGI(TAG, "Configure GPIO for lock");
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LOCK_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_set_level(LOCK_GPIO, 0);
}

void door_control_open(void)
{
    ESP_LOGI(TAG, "door_control_open: activating lock");
    gpio_set_level(LOCK_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(UNLOCK_TIME_MS));
    gpio_set_level(LOCK_GPIO, 0);
}

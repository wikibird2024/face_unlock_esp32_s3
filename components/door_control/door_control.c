#include "door_control.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "door_control";

#define LOCK_GPIO ((gpio_num_t)CONFIG_DOOR_LOCK_GPIO)
/* ESP32-S3: 22-25 do not exist, 26-37 are flash and octal PSRAM. */
#if CONFIG_DOOR_LOCK_GPIO >= 22 && CONFIG_DOOR_LOCK_GPIO <= 37
#error "CONFIG_DOOR_LOCK_GPIO is not usable on ESP32-S3 (22-37)"
#endif
/* A Kconfig bool set to "n" is not defined at all, so test it with #if. */
#if CONFIG_DOOR_LOCK_ACTIVE_HIGH
#define UNLOCKED_LEVEL 1
#define ACTIVE_LEVEL_NAME "high"
#else
#define UNLOCKED_LEVEL 0
#define ACTIVE_LEVEL_NAME "low"
#endif
#define LOCKED_LEVEL (1 - UNLOCKED_LEVEL)

static esp_timer_handle_t s_relock_timer;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_is_open;
/* When the current unlock ends. A relock callback that was already
 * dispatched before a new open() must not lock the new unlock early. */
static int64_t s_relock_at_us;

static void set_open(bool is_open) {
  taskENTER_CRITICAL(&s_lock);
  s_is_open = is_open;
  gpio_set_level(LOCK_GPIO, is_open ? UNLOCKED_LEVEL : LOCKED_LEVEL);
  taskEXIT_CRITICAL(&s_lock);
}

static void relock_timer_callback(void *argument) {
  (void)argument;
  taskENTER_CRITICAL(&s_lock);
  const bool is_due = esp_timer_get_time() >= s_relock_at_us;
  if (is_due) {
    s_is_open = false;
    gpio_set_level(LOCK_GPIO, LOCKED_LEVEL);
  }
  taskEXIT_CRITICAL(&s_lock);
  if (is_due) {
    ESP_LOGI(TAG, "Locked (hold time over)");
  }
}

esp_err_t door_control_init(void) {
  ESP_RETURN_ON_FALSE(s_relock_timer == NULL, ESP_ERR_INVALID_STATE, TAG,
                      "already initialised");

  /* Set the level before switching to output so the lock never pulses. */
  gpio_set_level(LOCK_GPIO, LOCKED_LEVEL);
  const gpio_config_t io_config = {
      .pin_bit_mask = 1ULL << CONFIG_DOOR_LOCK_GPIO,
      /* INPUT_OUTPUT so the level can be read back in self-tests. */
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&io_config), TAG, "gpio_config failed");
  set_open(false);

  const esp_timer_create_args_t timer_args = {
      .callback = relock_timer_callback,
      .name = "door_relock",
  };
  ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_relock_timer), TAG,
                      "timer create failed");
  ESP_LOGI(TAG, "Lock on GPIO %d, active %s", CONFIG_DOOR_LOCK_GPIO,
           ACTIVE_LEVEL_NAME);
  return ESP_OK;
}

esp_err_t door_control_open(uint32_t hold_ms) {
  ESP_RETURN_ON_FALSE(s_relock_timer != NULL, ESP_ERR_INVALID_STATE, TAG,
                      "not initialised");
  ESP_RETURN_ON_FALSE(hold_ms > 0, ESP_ERR_INVALID_ARG, TAG, "hold_ms is 0");
  esp_timer_stop(s_relock_timer); /* ESP_ERR_INVALID_STATE if idle: fine */
  taskENTER_CRITICAL(&s_lock);
  s_relock_at_us = esp_timer_get_time() + (int64_t)hold_ms * 1000;
  taskEXIT_CRITICAL(&s_lock);
  set_open(true);
  esp_err_t err =
      esp_timer_start_once(s_relock_timer, (uint64_t)hold_ms * 1000);
  if (err != ESP_OK) {
    set_open(false); /* never leave the door open without a timer */
    ESP_LOGE(TAG, "Relock timer failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "Unlocked for %u ms", (unsigned)hold_ms);
  return ESP_OK;
}

esp_err_t door_control_close(void) {
  ESP_RETURN_ON_FALSE(s_relock_timer != NULL, ESP_ERR_INVALID_STATE, TAG,
                      "not initialised");
  esp_timer_stop(s_relock_timer);
  if (door_control_is_open()) {
    ESP_LOGI(TAG, "Locked");
  }
  set_open(false);
  return ESP_OK;
}

bool door_control_is_open(void) {
  taskENTER_CRITICAL(&s_lock);
  const bool is_open = s_is_open;
  taskEXIT_CRITICAL(&s_lock);
  return is_open;
}

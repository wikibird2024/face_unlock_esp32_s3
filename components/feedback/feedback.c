#include "feedback.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_events.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "feedback";

#define BUZZER_GPIO CONFIG_FEEDBACK_BUZZER_GPIO
#define RGB_LED_GPIO CONFIG_FEEDBACK_RGB_LED_GPIO
/* ESP32-S3: 22-25 do not exist, 26-37 are flash and octal PSRAM. */
#if (BUZZER_GPIO >= 22 && BUZZER_GPIO <= 37) ||                                \
    (RGB_LED_GPIO >= 22 && RGB_LED_GPIO <= 37)
#error "A feedback GPIO is not usable on ESP32-S3 (22-37)"
#endif

#define SHORT_BEEP_MS 80
#define LONG_BEEP_MS 400
#define BEEP_GAP_MS 100
#define SHOW_RESULT_MS 1000
/* Out of 255: a WS2812 at full power is too bright to look at. */
#define LED_LEVEL 40
#define TASK_STACK_SIZE 3072
#define TASK_PRIORITY 2

typedef enum { LED_OFF, LED_GREEN, LED_RED, LED_BLUE, LED_YELLOW } led_color_t;

/* What to show for one event: the LED goes on, the beeps play, then the
 * LED stays on for led_after_ms (0 = until the next pattern). */
typedef struct {
  led_color_t led;
  uint8_t beep_count;
  uint16_t beep_ms;
  uint16_t led_after_ms;
} pattern_t;

/* Holds only the newest pattern: a new event cuts the old one short. */
static QueueHandle_t s_patterns;
/* NULL when no RGB LED is set. Used only by the feedback task after start. */
static led_strip_handle_t s_led;

static void set_pin(int gpio, bool is_on) {
  if (gpio >= 0) {
    gpio_set_level((gpio_num_t)gpio, is_on ? 1 : 0);
  }
}

/* Errors are ignored: a wrong LED color must never stop anything. */
static void set_led(led_color_t led) {
  if (s_led == NULL) {
    return;
  }
  if (led == LED_OFF) {
    led_strip_clear(s_led);
    return;
  }
  const bool has_red = led == LED_RED || led == LED_YELLOW;
  const bool has_green = led == LED_GREEN || led == LED_YELLOW;
  const bool has_blue = led == LED_BLUE;
  led_strip_set_pixel(s_led, 0, has_red ? LED_LEVEL : 0,
                      has_green ? LED_LEVEL : 0, has_blue ? LED_LEVEL : 0);
  led_strip_refresh(s_led);
}

/* Wait, but stop early when a new pattern arrives. True = stopped early. */
static bool wait_or_new_pattern(uint32_t wait_ms) {
  pattern_t next;
  return xQueuePeek(s_patterns, &next, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}

static void play(const pattern_t *pattern) {
  set_led(pattern->led);
  for (uint8_t i = 0; i < pattern->beep_count; i++) {
    set_pin(BUZZER_GPIO, true);
    const bool is_cut = wait_or_new_pattern(pattern->beep_ms);
    set_pin(BUZZER_GPIO, false);
    if (is_cut || wait_or_new_pattern(BEEP_GAP_MS)) {
      return;
    }
  }
  if (pattern->led_after_ms > 0 &&
      !wait_or_new_pattern(pattern->led_after_ms)) {
    set_led(LED_OFF);
  }
}

static void feedback_task(void *argument) {
  (void)argument;
  pattern_t pattern;
  for (;;) {
    xQueueReceive(s_patterns, &pattern, portMAX_DELAY);
    play(&pattern);
  }
}

static void on_app_event(void *handler_argument, esp_event_base_t base,
                         int32_t event_id, void *event_data) {
  (void)handler_argument;
  (void)base;
  (void)event_data;
  pattern_t pattern;
  switch ((app_event_id_t)event_id) {
  case APP_EVENT_DOOR_OPENED:
    pattern = (pattern_t){LED_GREEN, 1, SHORT_BEEP_MS, 0};
    break;
  case APP_EVENT_DOOR_CLOSED:
    pattern = (pattern_t){LED_OFF, 0, 0, 0};
    break;
  case APP_EVENT_APPROVAL_REQUESTED:
    pattern = (pattern_t){LED_YELLOW, 1, SHORT_BEEP_MS, 0};
    break;
  case APP_EVENT_FACE_REJECTED:
  case APP_EVENT_APPROVAL_DENIED:
  case APP_EVENT_APPROVAL_TIMEOUT:
    pattern = (pattern_t){LED_RED, 2, SHORT_BEEP_MS, SHOW_RESULT_MS};
    break;
  case APP_EVENT_ENROLL_STARTED:
    pattern = (pattern_t){LED_BLUE, 1, LONG_BEEP_MS, 0};
    break;
  case APP_EVENT_FACE_ENROLLED:
    pattern = (pattern_t){LED_GREEN, 3, SHORT_BEEP_MS, SHOW_RESULT_MS};
    break;
  case APP_EVENT_ENROLL_FAILED:
    pattern = (pattern_t){LED_RED, 1, LONG_BEEP_MS, SHOW_RESULT_MS};
    break;
  default:
    return;
  }
  /* Never blocks: this runs on the shared event loop task. */
  xQueueOverwrite(s_patterns, &pattern);
}

static esp_err_t start_buzzer(int gpio) {
  const gpio_config_t io_config = {
      .pin_bit_mask = 1ULL << gpio,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&io_config), TAG, "buzzer gpio failed");
  set_pin(gpio, false);
  return ESP_OK;
}

static esp_err_t start_rgb_led(void) {
  const led_strip_config_t strip_config = {
      .strip_gpio_num = RGB_LED_GPIO,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
  };
  const led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
  };
  ESP_RETURN_ON_ERROR(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led), TAG,
      "rgb led failed");
  set_led(LED_OFF);
  return ESP_OK;
}

esp_err_t feedback_start(void) {
  if (BUZZER_GPIO < 0 && RGB_LED_GPIO < 0) {
    ESP_LOGI(TAG, "Off (no buzzer or RGB LED GPIO in menuconfig)");
    return ESP_OK;
  }
  ESP_RETURN_ON_FALSE(s_patterns == NULL, ESP_ERR_INVALID_STATE, TAG,
                      "already started");
  if (BUZZER_GPIO >= 0) {
    ESP_RETURN_ON_ERROR(start_buzzer(BUZZER_GPIO), TAG, "buzzer");
  }
  if (RGB_LED_GPIO >= 0) {
    ESP_RETURN_ON_ERROR(start_rgb_led(), TAG, "rgb led");
  }

  s_patterns = xQueueCreate(1, sizeof(pattern_t));
  ESP_RETURN_ON_FALSE(s_patterns != NULL, ESP_ERR_NO_MEM, TAG, "no queue");
  ESP_RETURN_ON_FALSE(xTaskCreate(feedback_task, "feedback", TASK_STACK_SIZE,
                                  NULL, TASK_PRIORITY, NULL) == pdPASS,
                      ESP_ERR_NO_MEM, TAG, "no task");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(APP_EVENTS, ESP_EVENT_ANY_ID,
                                                 on_app_event, NULL),
                      TAG, "event register failed");
  ESP_LOGI(TAG, "Buzzer GPIO %d, RGB LED GPIO %d", BUZZER_GPIO, RGB_LED_GPIO);
  return ESP_OK;
}

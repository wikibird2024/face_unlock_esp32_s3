#include "keypad_input.h"

#include "esp_log.h"

static const char *TAG = "keypad_input";

void keypad_input_init(void) {
  /* TODO(next milestone): pick free GPIOs in Kconfig, scan rows/columns,
   * debounce, PIN entry with lockout. */
  ESP_LOGW(TAG, "Keypad not implemented");
}

bool keypad_input_get_key(char *key) {
  (void)key;
  return false;
}

bool keypad_input_get_pin(char *pin_buffer, size_t max_len) {
  (void)pin_buffer;
  (void)max_len;
  return false;
}

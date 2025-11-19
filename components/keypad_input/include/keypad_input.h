#ifndef KEYPAD_INPUT_H
#define KEYPAD_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize keypad GPIOs and internal buffers
void keypad_input_init(void);

// Check if a key was pressed (non-blocking), returns true if key read
bool keypad_input_get_key(char *key);

// Optional: read full PIN input (blocking or callback-based)
bool keypad_input_get_pin(char *pin_buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // KEYPAD_INPUT_H

/**
 * @file keypad_input.h
 * @brief Matrix keypad for PIN fallback. NOT IMPLEMENTED YET (stub; not
 *        started by app_main). Its placeholder pins clash with the camera
 *        and lock pins and must be moved before use.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void keypad_input_init(void);

/** Non-blocking; true when a key was read into *key. */
bool keypad_input_get_key(char *key);

bool keypad_input_get_pin(char *pin_buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

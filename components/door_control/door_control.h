/**
 * @file door_control.h
 * @brief Drives the lock output (relay or MOSFET) on one GPIO.
 *
 * Fail-safe: door_control_open() starts an esp_timer that locks again
 * after `hold_ms` even if the caller never calls door_control_close().
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Configure the lock GPIO and set it to locked. */
esp_err_t door_control_init(void);

/** Unlock now and lock again after `hold_ms`. Calling it while open
 *  restarts the hold time. */
esp_err_t door_control_open(uint32_t hold_ms);

/** Lock now. Safe to call when already locked. */
esp_err_t door_control_close(void);

bool door_control_is_open(void);

#ifdef __cplusplus
}
#endif

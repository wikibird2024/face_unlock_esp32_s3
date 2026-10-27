/**
 * @file feedback.h
 * @brief Beeps and an RGB LED for the person at the door.
 *
 * Listens to APP_EVENTS only: system_logic does not know it exists, and it
 * never touches the lock. Pins are set in menuconfig; with both at -1
 * nothing starts.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Set up the pins and start listening. Needs the default event loop. */
esp_err_t feedback_start(void);

#ifdef __cplusplus
}
#endif

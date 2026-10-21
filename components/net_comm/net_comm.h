/**
 * @file net_comm.h
 * @brief Network reporting. Today: listens to APP_EVENTS and logs what it
 *        would publish. Planned: Wi-Fi + MQTT over TLS (next milestone).
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Register for APP_EVENTS. Needs the default event loop. */
esp_err_t net_comm_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @file net_comm.h
 * @brief Wi-Fi station plus network reporting. Listens to APP_EVENTS and
 *        logs what it would publish. Planned: MQTT over TLS
 *        (docs/mqtt_contract.md).
 *
 * Wi-Fi is optional: with an empty CONFIG_NET_COMM_WIFI_SSID nothing
 * network-related starts and the door logic is not affected. On a
 * disconnect it keeps retrying forever; the door never waits for Wi-Fi.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Longest "a.b.c.d" IPv4 string plus the 0 byte. */
#define NET_COMM_IP_MAX_LEN 16

/** Register for APP_EVENTS and, when an SSID is set, start Wi-Fi.
 *  Needs the default event loop and NVS. */
esp_err_t net_comm_init(void);

/** True when Wi-Fi is connected and has an IP address. */
bool net_comm_is_connected(void);

/** The device's IPv4 address as text, or "" when not connected.
 *  `out_ip` must hold at least NET_COMM_IP_MAX_LEN bytes. */
void net_comm_get_ip(char *out_ip);

#ifdef __cplusplus
}
#endif

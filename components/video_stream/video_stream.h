/**
 * @file video_stream.h
 * @brief Live camera view over HTTP for a browser or phone on the same
 *        Wi-Fi: `/` (viewer page), `/stream` (MJPEG), `/jpg` (snapshot).
 *
 * The stream costs CPU and PSRAM bandwidth only while someone watches.
 * It has no login: anyone on the Wi-Fi can watch (see README, hardening
 * milestone). Face recognition keeps running while streaming.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Start the HTTP server (core 0, port 80). Call after net_comm_init()
 *  and camera_driver_init(). Does nothing when turned off in menuconfig. */
esp_err_t video_stream_start(void);

#ifdef __cplusplus
}
#endif

/**
 * @file console_commands.h
 * @brief Serial console (USB or UART, as set in menuconfig) for setup and
 *        debugging: enroll, list, delete, unlock, status.
 */
#pragma once

#include "esp_err.h"
#include "face_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Start the REPL. `face_db` may be NULL if recognition failed to start;
 *  face commands then report that. */
esp_err_t console_commands_start(face_db_t *face_db);

#ifdef __cplusplus
}
#endif

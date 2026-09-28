/**
 * @file face_db_nvs.h
 * @brief Opens a face_db_t saved in its own NVS partition, shared safely
 *        between tasks (FreeRTOS mutex).
 */
#pragma once

#include "esp_err.h"
#include "face_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Mount the face NVS partition (CONFIG_FACE_DB_NVS_PARTITION) and load the
 * saved faces. The partition is erased and started empty if its layout is
 * from an older NVS version or has no free pages.
 *
 * @param embedding_len length reported by the face model; saved faces with
 *                      another length are skipped (see face_db_skipped_count).
 */
esp_err_t face_db_nvs_open(size_t embedding_len, face_db_t **out_db);

#ifdef __cplusplus
}
#endif

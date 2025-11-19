
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type khi có face được nhận diện.
 * @param matched true nếu face match database, false nếu không.
 */
typedef void (*ai_face_recognition_cb_t)(bool matched);

/**
 * @brief Initialize AI face recognition component.
 *        Load BlazeFace & MobileFaceNet models from flash/PSRAM.
 * @return ESP_OK if success, ESP_FAIL otherwise.
 */
esp_err_t ai_face_recognition_init(void);

/**
 * @brief Set callback function để system_logic nhận kết quả face recognition
 * @param cb callback function pointer
 */
void ai_face_recognition_set_callback(ai_face_recognition_cb_t cb);

/**
 * @brief Process a single camera frame for face detection & recognition.
 *        Non-blocking if possible, internal task handles inference.
 * @param frame pointer to frame buffer (e.g., RGB888/QVGA)
 */
void ai_face_recognition_process_frame(void *frame);

#ifdef __cplusplus
}
#endif

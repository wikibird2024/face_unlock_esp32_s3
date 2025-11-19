#ifndef DATA_HANDLER_H
#define DATA_HANDLER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Data Structures -------------------------------------------------------

/**
 * @brief Structure to hold image frame data and metadata.
 * @note The 'frame' pointer holds dynamically allocated memory (malloc) and
 * MUST be freed by the consuming task using data_handler_free_frame().
 */
typedef struct {
  uint8_t *frame;
  size_t len;
  uint32_t timestamp;
} camera_frame_t;

/**
 * @brief Structure to hold face embedding data and metadata.
 * @note The 'embedding' pointer holds dynamically allocated memory (malloc) and
 * MUST be freed by the consuming task using data_handler_free_embedding().
 */
typedef struct {
  float *embedding;
  size_t len;
  uint32_t timestamp;
} face_embedding_t;

// --- Public API Functions --------------------------------------------------

/**
 * @brief Initializes the data handler component (creates FreeRTOS queues).
 * @return ESP_OK on success, ESP_ERR_NO_MEM if queue creation fails.
 */
esp_err_t data_handler_init(void);

/**
 * @brief Pushes a camera frame into the queue. This function dynamically
 * allocates memory for the frame data.
 * @param frame Pointer to the source frame data.
 * @param len Size of the frame in bytes.
 * @param timestamp Timestamp when the frame was captured.
 * @return ESP_OK on success.
 * ESP_ERR_NO_MEM if memory allocation fails.
 * ESP_ERR_NO_BUF if the queue is full (non-blocking push).
 */
esp_err_t data_handler_push_frame(uint8_t *frame, size_t len,
                                  uint32_t timestamp);

/**
 * @brief Pops a camera frame from the handler queue.
 * @note The receiving task is RESPONSIBLE for calling data_handler_free_frame()
 * once the data in the 'frame' pointer is processed.
 * @param frame Pointer to the camera_frame_t structure to receive the data.
 * @param timeout Timeout in milliseconds (will be converted to FreeRTOS ticks).
 * @return ESP_OK on successful reception.
 * ESP_ERR_INVALID_ARG if arguments are invalid.
 * ESP_ERR_TIMEOUT if the queue is empty after waiting.
 */
esp_err_t data_handler_pop_frame(camera_frame_t *frame, uint32_t timeout);

/**
 * @brief Frees the dynamically allocated memory inside the camera_frame_t
 * structure.
 * @param frame Pointer to the camera_frame_t structure.
 */
void data_handler_free_frame(camera_frame_t *frame);

/**
 * @brief Pushes a face embedding into the queue. This function dynamically
 * allocates memory for the embedding data.
 * @param embedding Pointer to the source embedding data (float array).
 * @param len Length of the embedding array (number of floats).
 * @param timestamp Timestamp when the embedding was generated.
 * @return ESP_OK on success.
 * ESP_ERR_NO_MEM if memory allocation fails.
 * ESP_ERR_NO_BUF if the queue is full (non-blocking push).
 */
esp_err_t data_handler_push_embedding(float *embedding, size_t len,
                                      uint32_t timestamp);

/**
 * @brief Pops a face embedding from the handler queue.
 * @note The receiving task is RESPONSIBLE for calling
 * data_handler_free_embedding() once the data in the 'embedding' pointer is
 * processed.
 * @param embedding Pointer to the face_embedding_t structure to receive the
 * data.
 * @param timeout Timeout in milliseconds (will be converted to FreeRTOS ticks).
 * @return ESP_OK on successful reception.
 * ESP_ERR_INVALID_ARG if arguments are invalid.
 * ESP_ERR_TIMEOUT if the queue is empty after waiting.
 */
esp_err_t data_handler_pop_embedding(face_embedding_t *embedding,
                                     uint32_t timeout);

/**
 * @brief Frees the dynamically allocated memory inside the face_embedding_t
 * structure.
 * @param embedding Pointer to the face_embedding_t structure.
 */
void data_handler_free_embedding(face_embedding_t *embedding);

#ifdef __cplusplus
}
#endif

#endif /* DATA_HANDLER_H */

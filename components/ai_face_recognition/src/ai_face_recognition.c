
#include "ai_face_recognition.h"
#include "esp_dl/esp_dl.h" // ESP-DL library
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ai_face_recognition";

/* ESP-DL models */
static esp_dl_model_t blazeface_model;
static esp_dl_model_t mobilefacenet_model;

/* Callback to system_logic */
static ai_face_recognition_cb_t face_cb = NULL;

/* Internal function to match embedding with stored database */
static bool match_embedding(const float *embedding, size_t len) {
  // TODO: implement real matching with stored embeddings
  // For demo, always return false
  (void)embedding;
  (void)len;
  return false;
}

esp_err_t ai_face_recognition_init(void) {
  ESP_LOGI(TAG, "Loading BlazeFace & MobileFaceNet models...");

  // Load BlazeFace model
  if (esp_dl_model_load("blazeface.dat", &blazeface_model) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to load BlazeFace model");
    return ESP_FAIL;
  }

  // Load MobileFaceNet embedding model
  if (esp_dl_model_load("mobilefacenet.dat", &mobilefacenet_model) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to load MobileFaceNet model");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "AI Face Recognition initialized successfully");
  return ESP_OK;
}

void ai_face_recognition_set_callback(ai_face_recognition_cb_t cb) {
  face_cb = cb;
}

void ai_face_recognition_process_frame(void *frame) {
  if (!frame)
    return;

  ESP_LOGD(TAG, "Processing frame for face recognition...");

  // 1. Face detection with BlazeFace
  detection_result_t faces = esp_dl_blazeface_detect(&blazeface_model, frame);

  if (faces.count == 0) {
    ESP_LOGD(TAG, "No faces detected");
    if (face_cb)
      face_cb(false);
    return;
  }

  // 2. Extract embedding with MobileFaceNet (first face only)
  float embedding[512]; // MobileFaceNet embedding size
  esp_dl_mobilefacenet_embed(&mobilefacenet_model, &faces[0], embedding);

  // 3. Match with stored embeddings
  bool matched = match_embedding(embedding, sizeof(embedding) / sizeof(float));

  // 4. Callback to system_logic
  if (face_cb) {
    face_cb(matched);
  }

  ESP_LOGI(TAG, "Frame processed, matched=%s", matched ? "true" : "false");
}

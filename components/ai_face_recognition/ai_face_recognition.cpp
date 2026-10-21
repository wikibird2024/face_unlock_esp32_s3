#include "ai_face_recognition.h"

#include <algorithm>
#include <cstring>
#include <new>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

static const char *TAG = "ai_face";

// Rough need of the default models (MSR+MNP detect, MFN feature) with their
// working buffers. Lower it if a board logs that less is enough.
static constexpr size_t MIN_FREE_PSRAM_BYTES = 2 * 1024 * 1024;

static HumanFaceDetect *s_detector = nullptr;
static HumanFaceFeat *s_feature_model = nullptr;
static size_t s_embedding_len = 0;

extern "C" esp_err_t ai_face_recognition_init(void) {
  ESP_RETURN_ON_FALSE(s_detector == nullptr, ESP_ERR_INVALID_STATE, TAG,
                      "already initialised");
  const int64_t start_us = esp_timer_get_time();

  // ESP-DL allocates inside its constructors with plain `new`; with C++
  // exceptions off, running out of memory there aborts (reboot loop). Check
  // first so a board without enough PSRAM boots with recognition off.
  const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  if (free_psram < MIN_FREE_PSRAM_BYTES) {
    ESP_LOGE(TAG, "Only %u bytes PSRAM free, models need about %u",
             static_cast<unsigned>(free_psram),
             static_cast<unsigned>(MIN_FREE_PSRAM_BYTES));
    return ESP_ERR_NO_MEM;
  }

  // lazy_load = false: load now so a missing model shows at boot, not at
  // the first face.
  s_detector = new (std::nothrow)
      HumanFaceDetect(static_cast<HumanFaceDetect::model_type_t>(
                          CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
                      false);
  s_feature_model = new (std::nothrow)
      HumanFaceFeat(static_cast<HumanFaceFeat::model_type_t>(
                        CONFIG_DEFAULT_HUMAN_FACE_FEAT_MODEL),
                    false);
  if (s_detector == nullptr || s_feature_model == nullptr) {
    delete s_detector;
    delete s_feature_model;
    s_detector = nullptr;
    s_feature_model = nullptr;
    ESP_LOGE(TAG, "Out of memory loading models (is PSRAM enabled?)");
    return ESP_ERR_NO_MEM;
  }

  const int feature_len = s_feature_model->get_feat_len();
  ESP_RETURN_ON_FALSE(feature_len > 0, ESP_FAIL, TAG,
                      "feature model reports length %d", feature_len);
  s_embedding_len = static_cast<size_t>(feature_len);

  ESP_LOGI(TAG, "Models loaded in %lld ms, embedding length %u",
           (esp_timer_get_time() - start_us) / 1000,
           static_cast<unsigned>(s_embedding_len));
  return ESP_OK;
}

extern "C" size_t ai_face_recognition_embedding_len(void) {
  return s_embedding_len;
}

extern "C" esp_err_t ai_face_recognition_run(const uint8_t *rgb565_image,
                                             uint16_t width, uint16_t height,
                                             ai_face_result_t *result,
                                             float *embedding,
                                             size_t embedding_capacity) {
  ESP_RETURN_ON_FALSE(s_detector != nullptr, ESP_ERR_INVALID_STATE, TAG,
                      "not initialised");
  ESP_RETURN_ON_FALSE(rgb565_image != nullptr && result != nullptr &&
                          embedding != nullptr && width > 0 && height > 0,
                      ESP_ERR_INVALID_ARG, TAG, "bad argument");
  ESP_RETURN_ON_FALSE(embedding_capacity >= s_embedding_len,
                      ESP_ERR_INVALID_SIZE, TAG, "embedding buffer too small");

  const int64_t start_us = esp_timer_get_time();
  std::memset(result, 0, sizeof(*result));

  dl::image::img_t image;
  // ESP-DL takes a non-const pointer but only reads the input image.
  image.data = const_cast<uint8_t *>(rgb565_image);
  image.width = width;
  image.height = height;
  image.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE;

  std::list<dl::detect::result_t> &faces = s_detector->run(image);
  result->face_count = static_cast<uint16_t>(faces.size());
  if (faces.empty()) {
    result->inference_ms =
        static_cast<uint32_t>((esp_timer_get_time() - start_us) / 1000);
    return ESP_OK;
  }

  // Same rule as ESP-DL's HumanFaceRecognizer: use the biggest face.
  const auto biggest = std::max_element(
      faces.begin(), faces.end(),
      [](const dl::detect::result_t &a, const dl::detect::result_t &b) {
        return a.box_area() < b.box_area();
      });
  if (biggest->keypoint.size() < 10) {
    ESP_LOGW(TAG, "Face without landmarks, skipped");
    result->inference_ms =
        static_cast<uint32_t>((esp_timer_get_time() - start_us) / 1000);
    return ESP_OK;
  }

  dl::TensorBase *feature = s_feature_model->run(image, biggest->keypoint);
  ESP_RETURN_ON_FALSE(feature != nullptr && feature->data != nullptr, ESP_FAIL,
                      TAG, "feature model returned nothing");
  std::memcpy(embedding, feature->data, s_embedding_len * sizeof(float));

  result->has_face = true;
  result->detect_score = biggest->score;
  for (size_t i = 0; i < 4 && i < biggest->box.size(); i++) {
    result->box[i] = static_cast<int16_t>(biggest->box[i]);
  }
  result->embedding_len = s_embedding_len;
  result->inference_ms =
      static_cast<uint32_t>((esp_timer_get_time() - start_us) / 1000);
  return ESP_OK;
}

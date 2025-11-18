#include "ai_face_recognition.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "ai_face_recognition";

void ai_face_recognition_init(void)
{
    ESP_LOGI(TAG, "Initialize esp-dl models (BlazeFace + MobileFaceNet)");
}

bool ai_face_recognition_process(void *frame)
{
    ESP_LOGI(TAG, "Process frame for face recognition");
    return false; // TODO: implement detection & embedding match
}

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>

#include "ai_face_recognition.h"
#include "camera_driver.h"
#include "door_control.h"
#include "keypad_input.h"
#include "net_comm.h"

static const char *TAG = "APP_MAIN";

void app_main(void) {
  ESP_LOGI(TAG, "Starting ESP32-S3 Face Unlock system");

  // Initialize subsystems
  camera_driver_init();
  ai_face_recognition_init();
  net_comm_init();
  door_control_init();

  // Main loop
  while (true) {
    camera_frame_t *frame = camera_driver_get_frame();
    if (frame != NULL) {
      bool recognized = ai_face_recognition_process(frame);
      camera_driver_release_frame(frame);

      if (recognized) {
        ESP_LOGI(TAG, "Face recognized — unlocking door.");
        door_control_open();
        net_comm_publish("face_unlock/status", "OPENED");
      } else {
        ESP_LOGI(TAG, "Face not recognized.");
        net_comm_publish("face_unlock/status", "REJECTED");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

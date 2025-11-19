
#include "camera_driver.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "camera_driver";

// ------------------ Pin mapping for OV5640 ------------------
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

static camera_config_t camera_config;
static bool camera_initialized = false;

// ------------------ Frame size helper ------------------
static framesize_t get_camera_frame_size(void) {
#if CONFIG_CAMERA_FRAME_SIZE_UXGA
  return FRAMESIZE_UXGA;
#elif CONFIG_CAMERA_FRAME_SIZE_SXGA
  return FRAMESIZE_SXGA;
#elif CONFIG_CAMERA_FRAME_SIZE_XGA
  return FRAMESIZE_XGA;
#elif CONFIG_CAMERA_FRAME_SIZE_SVGA
  return FRAMESIZE_SVGA;
#elif CONFIG_CAMERA_FRAME_SIZE_VGA
  return FRAMESIZE_VGA;
#else
  return FRAMESIZE_SVGA;
#endif
}

// ------------------ Initialization ------------------
void camera_driver_init(void) {
  camera_config = (camera_config_t){
      .pin_d0 = CAM_PIN_D0,
      .pin_d1 = CAM_PIN_D1,
      .pin_d2 = CAM_PIN_D2,
      .pin_d3 = CAM_PIN_D3,
      .pin_d4 = CAM_PIN_D4,
      .pin_d5 = CAM_PIN_D5,
      .pin_d6 = CAM_PIN_D6,
      .pin_d7 = CAM_PIN_D7,
      .pin_xclk = CAM_PIN_XCLK,
      .pin_pclk = CAM_PIN_PCLK,
      .pin_vsync = CAM_PIN_VSYNC,
      .pin_href = CAM_PIN_HREF,
      .pin_sccb_sda = CAM_PIN_SIOD,
      .pin_sccb_scl = CAM_PIN_SIOC,
      .pin_pwdn = -1,
      .pin_reset = -1,
      .xclk_freq_hz = 20000000,
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,
      .pixel_format = PIXFORMAT_JPEG,
      .frame_size = get_camera_frame_size(),
      .jpeg_quality = CONFIG_CAMERA_JPEG_QUALITY,
      .fb_count = 2,
      .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
      .fb_location = CAMERA_FB_IN_PSRAM,
  };

  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
    camera_initialized = false;
  } else {
    camera_initialized = true;
    ESP_LOGI(TAG, "Camera initialized successfully");
  }
}

// ------------------ Frame capture / release ------------------
camera_frame_t *camera_driver_get_frame(void) {
  if (!camera_initialized) {
    ESP_LOGW(TAG, "Camera not initialized!");
    return NULL;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
    return NULL;

  camera_frame_t *frame = malloc(sizeof(camera_frame_t));
  if (!frame) {
    esp_camera_fb_return(fb);
    return NULL;
  }

  frame->fb = fb;
  frame->width = fb->width;
  frame->height = fb->height;

  return frame;
}

void camera_driver_release_frame(camera_frame_t *frame) {
  if (!frame)
    return;
  if (frame->fb)
    esp_camera_fb_return((camera_fb_t *)frame->fb);
  free(frame);
}

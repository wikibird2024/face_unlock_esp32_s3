#include "camera_driver.h"

#include <stdbool.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "camera_driver";

/* A Kconfig bool set to "n" is not defined at all, so test it with #if. */
#if CONFIG_CAMERA_VFLIP
#define VFLIP_ENABLED 1
#else
#define VFLIP_ENABLED 0
#endif
#if CONFIG_CAMERA_HMIRROR
#define HMIRROR_ENABLED 1
#else
#define HMIRROR_ENABLED 0
#endif

static bool s_is_initialised;

static framesize_t configured_frame_size(void) {
#if CONFIG_CAMERA_FRAME_SIZE_QVGA
  return FRAMESIZE_QVGA;
#elif CONFIG_CAMERA_FRAME_SIZE_HVGA
  return FRAMESIZE_HVGA;
#elif CONFIG_CAMERA_FRAME_SIZE_VGA
  return FRAMESIZE_VGA;
#else
#error "Pick a frame size in menuconfig: Camera driver"
#endif
}

esp_err_t camera_driver_init(void) {
  ESP_RETURN_ON_FALSE(!s_is_initialised, ESP_ERR_INVALID_STATE, TAG,
                      "already initialised");

  const camera_config_t config = {
      .pin_pwdn = CONFIG_CAMERA_PIN_PWDN,
      .pin_reset = CONFIG_CAMERA_PIN_RESET,
      .pin_xclk = CONFIG_CAMERA_PIN_XCLK,
      .pin_sccb_sda = CONFIG_CAMERA_PIN_SIOD,
      .pin_sccb_scl = CONFIG_CAMERA_PIN_SIOC,
      .pin_d7 = CONFIG_CAMERA_PIN_D7,
      .pin_d6 = CONFIG_CAMERA_PIN_D6,
      .pin_d5 = CONFIG_CAMERA_PIN_D5,
      .pin_d4 = CONFIG_CAMERA_PIN_D4,
      .pin_d3 = CONFIG_CAMERA_PIN_D3,
      .pin_d2 = CONFIG_CAMERA_PIN_D2,
      .pin_d1 = CONFIG_CAMERA_PIN_D1,
      .pin_d0 = CONFIG_CAMERA_PIN_D0,
      .pin_vsync = CONFIG_CAMERA_PIN_VSYNC,
      .pin_href = CONFIG_CAMERA_PIN_HREF,
      .pin_pclk = CONFIG_CAMERA_PIN_PCLK,
      .xclk_freq_hz = CONFIG_CAMERA_XCLK_FREQ_HZ,
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,
      .pixel_format = PIXFORMAT_RGB565,
      .frame_size = configured_frame_size(),
      .jpeg_quality = 12, /* unused for RGB565 */
      .fb_count = 2,
      .fb_location = CAMERA_FB_IN_PSRAM,
      /* Always hand out the newest frame so recognition is not behind. */
      .grab_mode = CAMERA_GRAB_LATEST,
      .sccb_i2c_port = 0,
  };

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG,
             "Camera init failed: %s (0x%x). Check wiring, pins in "
             "menuconfig and that PSRAM is enabled.",
             esp_err_to_name(err), err);
    return err;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor != NULL) {
    ESP_LOGI(TAG, "Sensor PID 0x%04x", sensor->id.PID);
    sensor->set_vflip(sensor, VFLIP_ENABLED);
    sensor->set_hmirror(sensor, HMIRROR_ENABLED);
  }
  s_is_initialised = true;
  ESP_LOGI(TAG, "Camera ready");
  return ESP_OK;
}

esp_err_t camera_driver_get_frame(camera_frame_t *out_frame) {
  ESP_RETURN_ON_FALSE(out_frame != NULL, ESP_ERR_INVALID_ARG, TAG, "no output");
  ESP_RETURN_ON_FALSE(s_is_initialised, ESP_ERR_INVALID_STATE, TAG,
                      "not initialised");

  camera_fb_t *frame_buffer = esp_camera_fb_get();
  if (frame_buffer == NULL) {
    ESP_LOGW(TAG, "Frame capture failed");
    return ESP_FAIL;
  }
  if (frame_buffer->format != PIXFORMAT_RGB565) {
    ESP_LOGE(TAG, "Unexpected pixel format %d", frame_buffer->format);
    esp_camera_fb_return(frame_buffer);
    return ESP_FAIL;
  }
  *out_frame = (camera_frame_t){
      .data = frame_buffer->buf,
      .length_bytes = frame_buffer->len,
      .width = (uint16_t)frame_buffer->width,
      .height = (uint16_t)frame_buffer->height,
      .timestamp_us = (int64_t)frame_buffer->timestamp.tv_sec * 1000000 +
                      frame_buffer->timestamp.tv_usec,
      .driver_frame = frame_buffer,
  };
  return ESP_OK;
}

void camera_driver_release_frame(camera_frame_t *frame) {
  if (frame == NULL || frame->driver_frame == NULL) {
    return;
  }
  esp_camera_fb_return((camera_fb_t *)frame->driver_frame);
  memset(frame, 0, sizeof(*frame));
}

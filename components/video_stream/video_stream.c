#include "video_stream.h"

#if CONFIG_VIDEO_STREAM_ENABLE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_driver.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net_comm.h"

static const char *TAG = "video_stream";

#define FRAME_DELAY_MS (1000 / CONFIG_VIDEO_STREAM_MAX_FPS)
#define PART_BOUNDARY "frameboundary"

/* Only one viewer at a time: a second stream would starve recognition. */
static volatile bool s_stream_busy;

static const char VIEWER_PAGE[] =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Face unlock camera</title>"
    "<style>body{margin:0;background:#111;color:#eee;font-family:sans-serif;"
    "text-align:center}img{width:100%;max-width:640px}</style></head>"
    "<body><h3>Face unlock camera</h3>"
    "<img src=\"/stream\" alt=\"live camera\">"
    "<p><a style=\"color:#8cf\" href=\"/jpg\">snapshot</a></p>"
    "</body></html>";

static esp_err_t handle_viewer_page(httpd_req_t *request) {
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, VIEWER_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_current_frame_jpeg(httpd_req_t *request,
                                         uint8_t **out_jpeg,
                                         size_t *out_length_bytes) {
  camera_frame_t frame;
  ESP_RETURN_ON_ERROR(camera_driver_get_frame(&frame), TAG, "no frame");
  esp_err_t err = camera_driver_frame_to_jpeg(
      &frame, CONFIG_VIDEO_STREAM_JPEG_QUALITY, out_jpeg, out_length_bytes);
  camera_driver_release_frame(&frame);
  (void)request;
  return err;
}

static esp_err_t handle_snapshot(httpd_req_t *request) {
  uint8_t *jpeg = NULL;
  size_t length_bytes = 0;
  if (send_current_frame_jpeg(request, &jpeg, &length_bytes) != ESP_OK) {
    return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "camera busy or not running");
  }
  httpd_resp_set_type(request, "image/jpeg");
  esp_err_t err =
      httpd_resp_send(request, (const char *)jpeg, (ssize_t)length_bytes);
  free(jpeg);
  return err;
}

static esp_err_t handle_stream(httpd_req_t *request) {
  if (s_stream_busy) {
    /* esp_http_server has no 503 code of its own. */
    httpd_resp_set_status(request, "503 Service Unavailable");
    return httpd_resp_sendstr(request, "Someone is already watching.");
  }
  s_stream_busy = true;
  ESP_LOGI(TAG, "Viewer connected");
  httpd_resp_set_type(request,
                      "multipart/x-mixed-replace;boundary=" PART_BOUNDARY);

  esp_err_t err = ESP_OK;
  while (err == ESP_OK) {
    uint8_t *jpeg = NULL;
    size_t length_bytes = 0;
    err = send_current_frame_jpeg(request, &jpeg, &length_bytes);
    if (err != ESP_OK) {
      break;
    }
    char part_header[96];
    const int header_length =
        snprintf(part_header, sizeof(part_header),
                 "\r\n--" PART_BOUNDARY "\r\n"
                 "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                 (unsigned)length_bytes);
    err = httpd_resp_send_chunk(request, part_header, header_length);
    if (err == ESP_OK) {
      err = httpd_resp_send_chunk(request, (const char *)jpeg,
                                  (ssize_t)length_bytes);
    }
    free(jpeg);
    vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
  }
  /* err != ESP_OK usually means the viewer closed the page. */
  ESP_LOGI(TAG, "Viewer gone (%s)", esp_err_to_name(err));
  s_stream_busy = false;
  return err;
}

esp_err_t video_stream_start(void) {
  if (CONFIG_NET_COMM_WIFI_SSID[0] == '\0') {
    ESP_LOGI(TAG, "Not started: Wi-Fi is off (menuconfig: Network)");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.core_id = 0; /* recognition owns core 1 */
  config.stack_size = 8192;

  httpd_handle_t server = NULL;
  ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "http server");

  const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = handle_viewer_page},
      {.uri = "/stream", .method = HTTP_GET, .handler = handle_stream},
      {.uri = "/jpg", .method = HTTP_GET, .handler = handle_snapshot},
  };
  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &routes[i]), TAG,
                        "route %s", routes[i].uri);
  }
  ESP_LOGI(TAG, "Live view on http://<ip>/ (port %d)", config.server_port);
  return ESP_OK;
}

#else /* !CONFIG_VIDEO_STREAM_ENABLE */

esp_err_t video_stream_start(void) { return ESP_OK; }

#endif

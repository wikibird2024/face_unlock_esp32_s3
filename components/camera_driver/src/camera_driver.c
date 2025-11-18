#include "camera_driver.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"

static const char *TAG = "camera_driver";

typedef struct {
    void *fb;
    int width;
    int height;
} camera_frame_t;

void camera_driver_init(void)
{
    ESP_LOGI(TAG, "camera_driver_init: configure OV5640 and camera peripheral");
    // TODO: initialize esp_camera_config_t & esp_camera_init()
}

camera_frame_t* camera_driver_get_frame(void)
{
    return NULL; // TODO: implement frame capture
}

void camera_driver_release_frame(camera_frame_t *frame)
{
    // TODO: release frame
}

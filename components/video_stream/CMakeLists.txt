idf_component_register(
    SRCS "video_stream.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES camera_driver esp_http_server net_comm
)

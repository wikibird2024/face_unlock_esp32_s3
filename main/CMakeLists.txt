idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES ai_face_recognition camera_driver console_commands
                  door_control esp_app_format esp_event face_db net_comm
                  nvs_flash system_logic
)

idf_component_register(
    SRCS "src/system_logic.c" "src/unlock_state_machine.c"
    INCLUDE_DIRS "include"
    REQUIRES face_db
    PRIV_REQUIRES ai_face_recognition app_events camera_driver door_control
                  esp_driver_gpio esp_timer
)

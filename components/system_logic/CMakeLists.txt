idf_component_register(
    SRCS "system_logic.c" "unlock_state_machine.c"
    INCLUDE_DIRS "."
    REQUIRES face_db
    PRIV_REQUIRES ai_face_recognition app_events camera_driver door_control
                  esp_driver_gpio esp_timer
)

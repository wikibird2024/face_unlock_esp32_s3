idf_component_register(
    SRCS "src/ai_face_recognition.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES human_face_detect human_face_recognition esp_timer
)

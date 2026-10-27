idf_component_register(
    SRCS "feedback.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES app_events esp_driver_gpio esp_event
)

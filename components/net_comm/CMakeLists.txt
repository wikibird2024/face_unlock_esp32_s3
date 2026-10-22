idf_component_register(
    SRCS "net_comm.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES app_events esp_event esp_netif esp_wifi
)

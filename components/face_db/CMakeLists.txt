idf_component_register(
    SRCS "face_db.c" "face_db_nvs.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash
)

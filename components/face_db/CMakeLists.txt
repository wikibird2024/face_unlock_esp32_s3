idf_component_register(
    SRCS "src/face_db.c" "src/face_db_nvs.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash
)

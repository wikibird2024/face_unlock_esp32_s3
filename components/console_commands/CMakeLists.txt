idf_component_register(
    SRCS "src/console_commands.c"
    INCLUDE_DIRS "include"
    REQUIRES face_db
    PRIV_REQUIRES console door_control system_logic
)

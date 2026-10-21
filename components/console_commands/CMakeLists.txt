idf_component_register(
    SRCS "console_commands.c"
    INCLUDE_DIRS "."
    REQUIRES face_db
    PRIV_REQUIRES console door_control system_logic
)

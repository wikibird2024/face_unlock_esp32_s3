# On-device tests for the parts that need real hardware (NVS, GPIO, timer).
#
#   cd test/target && idf.py build flash monitor
#
# Only the components under test are added, so the camera and ESP-DL are
# not built.
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components/face_db"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/door_control")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(face_unlock_target_tests)

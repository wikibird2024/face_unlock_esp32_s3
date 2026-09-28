# Host unit tests for the plain-C modules (no ESP-IDF, no board needed).
#
#   cmake -S test/host -B build-host && cmake --build build-host
#   ctest --test-dir build-host --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(face_unlock_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

option(HOST_TESTS_SANITIZE "Build with address and undefined-behaviour sanitizers" ON)

add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_FLOAT UNITY_INCLUDE_DOUBLE)

set(STRICT_WARNINGS -Wall -Wextra -Werror -Wshadow -Wconversion -Wno-sign-conversion)

function(add_host_test name)
  cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES" ${ARGN})
  add_executable(${name} ${ARG_SOURCES})
  target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
  target_link_libraries(${name} PRIVATE unity m)
  target_compile_options(${name} PRIVATE ${STRICT_WARNINGS})
  if(HOST_TESTS_SANITIZE)
    target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${name} PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

enable_testing()

add_host_test(test_face_db
  SOURCES test_face_db.c ${REPO_ROOT}/components/face_db/src/face_db.c
  INCLUDES ${REPO_ROOT}/components/face_db/include)

add_host_test(test_unlock_state_machine
  SOURCES test_unlock_state_machine.c
          ${REPO_ROOT}/components/system_logic/src/unlock_state_machine.c
  INCLUDES ${REPO_ROOT}/components/system_logic/include)

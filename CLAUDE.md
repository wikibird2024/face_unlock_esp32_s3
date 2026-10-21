# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Face unlock door lock on **ESP32-S3** (ESP-IDF, C with one C++ file).
OV5640 → ESP-DL face detect + embedding → cosine match against faces in
NVS → unlock state machine → lock GPIO. README.md has the diagram,
hardware table and console commands.

## Commands

```bash
# Host unit tests (no ESP-IDF needed) - run these after touching face_db or
# unlock_state_machine
cmake -S test/host -B build-host && cmake --build build-host
ctest --test-dir build-host --output-on-failure
./build-host/test_face_db                 # one suite

# Firmware (ESP-IDF v5.5.x; not installed on this dev machine - CI builds it)
. $IDF_PATH/export.sh && idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# On-device tests (NVS, GPIO, timer) - needs a board
cd test/target && idf.py build flash monitor

# Formatting (CI fails on a diff)
git ls-files 'components/*.[ch]' 'components/*.cpp' 'main/*.c' 'test/host/*.c' \
  'test/target/main/*.c' | xargs clang-format -i
```

## Architecture rules

- **Pure logic is plain C with no ESP-IDF includes** so it runs in host
  tests: `face_db/src/face_db.c` and `system_logic/src/unlock_state_machine.c`.
  Hardware and OS access goes in separate files (`face_db_nvs.c`,
  `system_logic.c`). Storage and locking reach `face_db` through the
  `face_db_storage_t` / `face_db_lock_t` function tables - keep it that way.
- **One task owns the state machine** (`system_logic.c`, task `face_logic`
  on core 1). Other tasks (console, button) only queue requests with
  `system_logic_request_*()`. `face_db` is shared and has its own mutex.
- **Outputs go out as events**: `system_logic` posts `APP_EVENTS`
  (`app_events.h`) on the default loop; new listeners (MQTT, buzzer, display)
  register a handler instead of being called from `system_logic`.
- **Door safety**: `door_control_open(hold_ms)` always arms an esp_timer
  that relocks; `app_main` initialises the lock before anything else.
  Never add a path that sets the lock open without the timer.
- `ai_face_recognition.cpp` is the only C++ file; it wraps the registry
  components `espressif/human_face_detect` and `human_face_recognition`
  (versions in `components/ai_face_recognition/idf_component.yml`). Camera
  frames are **RGB565 big-endian** → `DL_IMAGE_PIX_TYPE_RGB565BE`.
- Registry dependencies are declared in the component that uses them
  (`components/*/idf_component.yml`), not in `main/`.
- Faces live in their own NVS partition `face_nvs` (`partitions.csv`),
  one blob per face, key `f<id>`, header with a format version. Changing
  the record layout means bumping `RECORD_FORMAT_VERSION` in `face_db_nvs.c`.
- A Kconfig `bool` set to `n` is **undefined**, so test it with `#if`, never
  use it in a C expression.

## Known gaps

- Firmware is built only by the CI `firmware` job (passing); it has not
  been run on a board yet.
- Board assumptions in `sdkconfig.defaults` (8 MB flash, octal PSRAM) are
  not confirmed for the custom board.
- `keypad_input` is a stub with no pins chosen yet; pick them clear of the
  camera and lock pins (README "Next milestones").
- `net_comm` only logs events; no Wi-Fi/MQTT yet.
- `dependencies.lock` is not committed yet; add it from the first build.

# face_unlock_esp32_s3

Door lock that opens for enrolled faces. ESP32-S3 + OV5640 camera, face
detection and recognition with Espressif's ESP-DL models (the same models
[esp-who](https://github.com/espressif/esp-who) uses), faces stored in flash.

## How it works

```
 OV5640 ──RGB565 320x240──▶ camera_driver
                                  │ frame
                                  ▼
                        ai_face_recognition  (ESP-DL: detect face → 512-float embedding)
                                  │ embedding
                                  ▼
 console / BOOT button ─request─▶ system_logic task ◀──▶ face_db (cosine match, NVS "face_nvs")
                                  │ unlock_state_machine decides
                                  ▼
                            door_control (GPIO, relocks itself by timer)
                                  │
                           APP_EVENTS (esp_event) ──▶ net_comm (log today, MQTT later)
```

Unlock rules (`unlock_state_machine`):

- The same result must be seen on **3 frames in a row** before acting
  (stops one lucky frame from opening the door).
- Known face (similarity ≥ 0.55) → unlock for 3 s. Unknown face → reject.
- After an unlock, a rejection or an enroll: 2 s cooldown where faces are
  ignored.
- The lock output has its own timer, so it relocks even if the software
  stalls. At boot the lock is set to locked before anything else starts.

All numbers are in `idf.py menuconfig` → *System logic (unlock rules)*.

## Components

| Component | Job | Tested by |
|---|---|---|
| `camera_driver` | OV5640 via esp32-camera, RGB565, pins in Kconfig | device |
| `ai_face_recognition` | ESP-DL face detect + embedding (C++ inside, C API) | device |
| `face_db` | Enrolled faces: add, delete, best match; NVS storage | host + device |
| `system_logic` | Recognition task + unlock state machine | host (state machine) |
| `door_control` | Lock GPIO with auto-relock timer | device |
| `console_commands` | Serial commands for setup and debugging | manual |
| `app_events` | Event ids and payloads shared by all components | — |
| `net_comm` | Listens to events; MQTT is the next milestone | — |
| `keypad_input` | Stub, not started (next milestone) | — |

## Hardware

| Part | Default | Where to change |
|---|---|---|
| Module | ESP32-S3, ≥ 8 MB flash, **8 MB octal PSRAM** (N8R8 / N16R8) | `sdkconfig.defaults` |
| Camera | OV5640, XCLK 15, SDA 4, SCL 5, D0–D7 11 9 8 10 12 18 17 16, VSYNC 6, HREF 7, PCLK 13 | menuconfig → *Camera driver* |
| Lock output | GPIO 2, active high | menuconfig → *Door control* |
| Enroll button | GPIO 0 (BOOT), active low | menuconfig → *System logic* |

> Quad-PSRAM modules (N8R2, N4R2): set `CONFIG_SPIRAM_MODE_QUAD=y` in
> `sdkconfig.defaults` and delete `sdkconfig`, or the board stops at boot
> with a PSRAM error.

## Build and flash

Needs ESP-IDF **v5.5.x** (v5.3 or newer works for ESP-DL 3.3).

```bash
. $IDF_PATH/export.sh
idf.py build                    # first build downloads ESP-DL, models, esp32-camera
idf.py -p /dev/ttyUSB0 flash monitor
```

After changing `sdkconfig.defaults`, delete `sdkconfig` so the new defaults
are used. Commit the `dependencies.lock` that the first build creates so
everyone builds the same library versions.

## First run

In the serial monitor:

```
face> enroll alice        # then look at the camera for ~1 s
face> list
face> status              # live similarity: use it to tune the threshold
face> unlock              # manual override
face> delete 1
face> clear yes
face> help
```

Or hold the BOOT button to enroll the next face as `user_<n>`.
Enrolling the same person 2–3 times (different light or angle) makes
matching more reliable.

## Tests

| Suite | Runs on | Command |
|---|---|---|
| Host unit tests: `face_db`, `unlock_state_machine` (37 tests, ASan + UBSan) | PC | see below |
| On-device tests: `face_db` on NVS, `door_control` GPIO + timer | ESP32-S3 | `cd test/target && idf.py build flash monitor` |

```bash
cmake -S test/host -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
# one suite only:
./build-host/test_unlock_state_machine
```

CI (`.github/workflows/ci.yml`) runs the host tests, a clang-format check,
and builds the firmware and the on-device test app in the
`espressif/idf:v5.5.1` container.

## Next milestones

| Milestone | Content |
|---|---|
| Network + security | Wi-Fi provisioning, MQTT over TLS in `net_comm`, OTA (needs a two-slot partition table), NVS encryption for faces, secure boot |
| Keypad | Matrix or single-button keypad: pick GPIOs in Kconfig clear of the camera/lock pins, scan + debounce, key/PIN API or events, PIN unlock with lockout |
| Liveness | Reject photos of a face (blink check or IR camera) |


```markdown
# face_unlock_esp32_s3

ESP32-S3 Face Unlock System (OV5640 + ESP-DL BlazeFace & MobileFaceNet)

## Project Overview

This project implements a **modular, event-driven face unlock system** on ESP32-S3:

- Capture frames from OV5640 camera.
- Detect and recognize faces using **ESP-DL** (BlazeFace for detection, MobileFaceNet for embedding & matching).
- Control door lock via relay or servo.
- Optional network communication (WiFi + MQTT) for logging, telemetry, or remote control.
- Keyboard input (keypad) for manual override.
- Centralized system logic orchestrates AI results, input, and outputs.

The architecture is modular, making it easier to maintain, test, and extend.

---

## Folder Structure

```

.
├── CMakeLists.txt
├── components
│   ├── ai_face_recognition
│   ├── camera_driver
│   ├── data_handler
│   ├── door_control
│   ├── keypad_input
│   ├── net_comm
│   └── system_logic
├── idf_component.yml
├── main
│   ├── CMakeLists.txt
│   └── main.c
├── README.md
├── sdkconfig
└── sdkconfig.defaults

````

### Components

| Component               | Purpose |
|-------------------------|---------|
| **camera_driver**       | OV5640 initialization & frame capture |
| **ai_face_recognition** | ESP-DL wrappers: BlazeFace + MobileFaceNet |
| **data_handler**        | Queue/buffer for camera frames, embeddings, and intermediate data |
| **system_logic**        | Orchestrates AI results and input events to decide actions (unlock, alert, log) |
| **door_control**        | Relay/servo control to unlock/lock door, manage LEDs/Buzzer |
| **keypad_input**        | Capture user input, forward events to system_logic |
| **net_comm**            | WiFi/MQTT communication for logging, telemetry, or OTA updates |

---

## Getting Started

1. **Prerequisites**
   - ESP-IDF v5.x installed and `IDF_PATH` set.
   - Ensure PSRAM is enabled for high-resolution frames or AI models.

2. **Configure Project**
   ```bash
   idf.py menuconfig
````

* Adjust camera pins in `components/camera_driver/include/camera_driver.h`.
* Enable/disable logging or AI features via Kconfig.

3. **Build & Flash**

   ```bash
   idf.py build
   idf.py flash monitor
   ```

4. **Testing Flow**

   * Camera captures frames → pushed to `data_handler`.
   * AI processes frame → sends results to `system_logic`.
   * `system_logic` decides: unlock door via `door_control`, trigger alert, or log event.
   * Optional: keypad input can override AI.
   * Optional: network communication sends events or receives remote commands.

---

## Notes

* Logging can be controlled component-wise through Kconfig.
* Embedding database and AI threshold should be configurable.
* System designed to be modular: each component can be developed or replaced independently.
* Recommended: unit test each component separately before full integration.

---




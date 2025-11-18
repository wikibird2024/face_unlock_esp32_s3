# face_unlock_esp32_s3

ESP32-S3 Face Unlock skeleton (OV5640 + esp-dl)

Components:
- camera_driver : OV5640 initialization & frame capture
- ai_face_recognition : esp-dl wrappers (BlazeFace + MobileFaceNet)
- door_control : relay/servo control for lock
- net_comm : WiFi + MQTT (optional telemetry)

Generated on: 2025-11-18T12:46:26+07:00

Notes:
- Ensure ESP-IDF v5.x installed and IDF_PATH set.
- Adjust camera pins in components/camera_driver/include/camera_driver.h

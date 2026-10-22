# MQTT contract (planned, not implemented yet)

Author: Hao Tran

The contract between the ESP32-S3 firmware (`net_comm`) and the phone app
for the two-step unlock. The firmware side of the waiting state already
exists (`unlock_state_machine`, state `WAITING_APPROVAL`); today the
console commands `approve <id>` / `deny <id>` stand in for the phone.
Nothing below is implemented; this file records the design so both sides
are built against the same contract.

## Naming

`<device>` = `fu-` + last 6 hex digits of the ESP32's Wi-Fi MAC
(for example `fu-a1b2c3`). All topics start with `faceunlock/<device>/`.

## Topics

| Topic ...`/` | From → to | Payload (JSON) | Retained |
|---|---|---|---|
| `approval/request` | ESP → app | `{"request":1234,"face":4,"name":"alice","similarity":0.71,"expires_s":30}` | no |
| `approval/answer` | app → ESP | `{"request":1234,"approve":true,"phone":"p1","sig":"<hex>"}` | no |
| `event` | ESP → app | `{"type":"door_opened","name":"alice","time":"2026-10-22T08:02:11"}` | no |
| `status` | ESP → app | `"online"` / `"offline"` (last will) | yes |

## Answer signature

```
sig = HMAC-SHA256(phone_key, "<device>|<request>|<approve>")
```

The ESP32 opens the door only when all three hold:

1. `sig` is valid for the key of phone `<phone>` (keys from pairing,
   stored in NVS namespace `phones`; mbedTLS on the ESP32, the same
   test vectors in the app tests and the host tests).
2. Phone `<phone>` is linked to the face in the waiting request.
3. `request` is the id the state machine is waiting for right now
   (already enforced; ids start at a random value each boot).

## Pairing (phase C)

Console `pair <face_id>` → the ESP32 makes a key, shows it as a QR code /
code string → the app scans it and stores the key in the phone's secure
storage → the ESP32 saves phone → face in NVS.

## Phases

| Phase | Content |
|---|---|
| A | Wi-Fi + MQTT in `net_comm`: publish request/event/status, accept answers (test from a PC with mosquitto) |
| B | Flutter app skeleton (`app/`): connect, show request, Open/Deny |
| C | Pairing + HMAC check |
| D | Push notification (app closed) via a small cloud function |

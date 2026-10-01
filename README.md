# Smart Eco Station – Firmware

Two sketches that talk to the same Firebase database as the website
(`Desktop/eco station/Nyrc/smart eco station.html`).

| Folder | Board | Job |
|---|---|---|
| `EcoStation_Main/` | ESP32 DevKit | RFID, solenoid lock, servo, 2× ultrasonic, LCD, Wi-Fi → Firebase |
| `EcoStation_Cam/`  | ESP32-CAM (AI-Thinker) | **Option A:** classifies wet/dry on the CAM (Edge Impulse model) |
| `EcoStation_Cam_Server/` | ESP32-CAM (AI-Thinker) | **Option B:** sends photo to `ClassifierServer`, relays WET/DRY |
| `ClassifierServer/` | PC / Raspberry Pi | Flask server running a Teachable Machine model (`python app.py`) |

Flash **either** Option A or Option B onto the ESP32-CAM, not both. Both reply
to the main ESP32 in the same format, so the main sketch is unchanged.
For Option B: put `keras_model.h5` + `labels.txt` (Teachable Machine → Export →
Tensorflow → Keras) in `ClassifierServer/`, run `pip install -r requirements.txt`
then `python app.py`, and copy the URL it prints into `SERVER_URL`. The PC must
stay on and on the same Wi-Fi as the station.

## Wiring (Main ESP32)

| Part | Pin | ESP32 |
|---|---|---|
| RC522 RFID (3.3 V!) | SDA/SS · SCK · MOSI · MISO · RST | 5 · 18 · 23 · 19 · 27 |
| I2C LCD 16×2 | SDA · SCL | 21 · 22 |
| Servo MG996R (signal) | SIG | 13 (power from separate 5 V supply, common GND) |
| Solenoid lock | via relay/MOSFET IN | 26 (lock on 12 V supply + flyback diode) |
| Buzzer | + | 4 |
| Ultrasonic – WET bin | TRIG · ECHO | 32 · 33 |
| Ultrasonic – DRY bin | TRIG · ECHO | 25 · 14 |
| ESP32-CAM | CAM U0T (GPIO1) → RX · CAM U0R (GPIO3) ← TX | 16 · 17 |

- HC-SR04 ECHO outputs 5 V → use a 1 kΩ / 2 kΩ divider before the ESP32 pin.
- All grounds (ESP32, CAM, servo supply, 12 V supply) must be connected together.
- Disconnect the CAM's GPIO1/GPIO3 wires while uploading to the CAM.

## Setup

1. **Libraries** (Library Manager): `MFRC522`, `ESP32Servo`, `LiquidCrystal I2C`.
2. **Main sketch:** set `WIFI_SSID`, `WIFI_PASSWORD`, `STATION_ID`, `BIN_DEPTH_CM`.
   If your Firebase rules block unauthenticated writes, put a database secret in `FIREBASE_AUTH`.
3. **Camera model:** on [Edge Impulse](https://edgeimpulse.com) create an image-classification
   project with labels `wet` and `dry` (≈100+ photos each, taken by the ESP32-CAM on the flap),
   export as an **Arduino library**, add the .zip, and fix the `#include` at the top of
   `EcoStation_Cam.ino`.
4. Register cards on the website's Authority Portal. The UID format printed on
   the Serial Monitor (e.g. `A3 5F 8C 12`) is exactly what the website uses.

## What goes to Firebase

- `/users/{UID}/points` – +10 per sorted item
- `/bins/STATION-01` – `{ zone, columns: { wet, dry }, updated }` every 15 s → Authority Portal bin cards (warning ≥75 %, critical ≥95 %)
- `/logs` – scans, rewards and "bin full" alerts → portal event feed

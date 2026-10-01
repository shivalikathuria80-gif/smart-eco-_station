# Smart Eco Station: Components List & Wiring

Circuit diagram: [circuit_diagram.svg](circuit_diagram.svg) (open in any browser)

## 1. Components

### Electronics
| # | Component | Qty | Purpose |
|---|---|---|---|
| 1 | ESP32 DevKit V1 (30/38-pin) | 1 | Main controller + Wi-Fi |
| 2 | ESP32-CAM (AI-Thinker, OV2640) | 1 | Takes photo of waste |
| 3 | ESP32-CAM-MB USB programmer board | 1 | To upload code to the ESP32-CAM |
| 4 | RC522 RFID reader module | 1 | Reads user cards |
| 5 | RFID cards / key tags (13.56 MHz) | 5+ | One per user |
| 6 | 12V solenoid lock | 1 | Locks the lid |
| 7 | 1-channel 5V relay module | 1 | Lets the ESP32 switch the 12V solenoid |
| 8 | 1N4007 diode | 1 | Protects against solenoid voltage spikes |
| 9 | MG996R servo motor (metal gear) | 1 | Tilts the flap to wet/dry bin |
| 10 | HC-SR04 ultrasonic sensor | 2 | Measures fill level of each bin |
| 11 | 16×2 LCD with I2C module (PCF8574) | 1 | Shows messages and fill levels |
| 12 | Active buzzer (5V) | 1 | Beeps for success / error |
| 13 | Resistors 1 kΩ | 2 | Voltage divider for ECHO pins |
| 14 | Resistors 2 kΩ (or 2.2 kΩ) | 2 | Voltage divider for ECHO pins |
| 15 | Capacitor 1000 µF / 16V | 1 | Across servo 5V/GND, stops resets |

### Power
| # | Component | Qty | Purpose |
|---|---|---|---|
| 16 | 12V 2A DC adapter + DC jack | 1 | Main power |
| 17 | LM2596 buck converter (12V → 5V, 3A) | 1 | 5V for ESP32, CAM, servo, sensors |

### Wiring & build
| # | Component | Qty |
|---|---|---|
| 18 | Breadboard or dotted PCB | 1 |
| 19 | Jumper wires (M-M, M-F, F-F) | 40+ |
| 20 | Two dustbins (label: WET / DRY) | 2 |
| 21 | Flap / platform (cardboard, acrylic or MDF) | 1 |
| 22 | Enclosure / station body with lid | 1 |

### Other
| # | Item | Purpose |
|---|---|---|
| 23 | Laptop | Runs `app.py` (AI model) and shows the website |
| 24 | Wi-Fi router or phone hotspot | Connects ESP32, ESP32-CAM and laptop |

## 2. Pin connections

### Main ESP32
| Module | Module pin | ESP32 pin |
|---|---|---|
| RC522 | SDA (SS) | GPIO 5 |
| RC522 | SCK | GPIO 18 |
| RC522 | MOSI | GPIO 23 |
| RC522 | MISO | GPIO 19 |
| RC522 | RST | GPIO 27 |
| RC522 | 3.3V | **3V3** (not 5V!) |
| LCD I2C | SDA | GPIO 21 |
| LCD I2C | SCL | GPIO 22 |
| Buzzer | + | GPIO 4 |
| HC-SR04 WET | TRIG | GPIO 32 |
| HC-SR04 WET | ECHO | GPIO 33 (through 1k/2k divider) |
| HC-SR04 DRY | TRIG | GPIO 25 |
| HC-SR04 DRY | ECHO | GPIO 14 (through 1k/2k divider) |
| Servo | Signal (orange) | GPIO 13 |
| Relay | IN | GPIO 26 |
| ESP32-CAM | U0T (GPIO 1) | GPIO 16 (RX2) |
| ESP32-CAM | U0R (GPIO 3) | GPIO 17 (TX2) |

### Power
| From | To |
|---|---|
| 12V adapter + | LM2596 IN+ and Relay COM |
| Relay NO | Solenoid + |
| Solenoid − | 12V GND |
| LM2596 OUT+ (5V) | ESP32 VIN, ESP32-CAM 5V, Servo red, both HC-SR04 VCC, LCD VCC, Relay VCC |
| All GNDs | Connected together (common ground) |

## 3. ECHO voltage divider (for each HC-SR04)
The HC-SR04 ECHO pin outputs 5V, but ESP32 pins take 3.3V max.

```
ECHO ──[ 1kΩ ]──┬── ESP32 GPIO
                │
             [ 2kΩ ]
                │
               GND
```

## 4. Safety notes
- **Before connecting anything:** turn the LM2596 screw until its output reads **5.0V** on a multimeter.
- **Solenoid:** never connect it directly to the ESP32. Always go through the relay.
- **Diode:** place the 1N4007 across the solenoid with the stripe on the + side.
- **Servo:** power it from the 5V rail, not from the ESP32's 5V pin.
- **Uploading to the ESP32-CAM:** unplug its GPIO1/GPIO3 wires first.

## 5. Where to buy: Robu.in (single source)

Prices and stock were checked on robu.in on 29 Sep 2026 (₹, incl. GST) and may have changed since. Parts marked ⚠️ were **out of stock** then. For the cheap passives, the link is a Robu search page, because there are hundreds of near-identical listings.

| # | Component | Qty | Price (each) | Link |
|---|---|---|---|---|
| 1 | SmartElex ESP32-WROOM-32D DevKit | 1 | ₹819 | https://robu.in/product/smartelex-esp-wroom-32-wifi-bluetooth-networking-development-board-1-pcs/ |
| 2a | ESP32-CAM board **without** camera (AI-Thinker) | 1 | ₹709 | https://robu.in/product/esp32-cam-wifi-module-bluetooth-without-camera-module/ |
| 2b | OV2640 camera module, 24-pin ribbon ⚠️ | 1 | ₹279 | https://robu.in/product/0-3mp-ov2640-v1-0-camera-module-with-high-quality-sccb-connector/ |
| 3 | ESP32-CAM-MB USB programmer | 1 | ₹119 | https://robu.in/product/esp32-cam-mb-micro-usb-shield-module-for-esp32-cam-development-board/ |
| 4 | RC522 RFID reader + card & key tag | 1 | ₹69 | https://robu.in/product/mifare-rfid-readerwriter-13-56mhz-rc522-spi-s50-fudan-card-and-keychain/ |
| 5 | Extra 13.56 MHz RFID cards | 4+ | – | https://robu.in/?s=13.56mhz+rfid+card&post_type=product |
| 6 | 12V DC solenoid door lock (1240, 0.6A) | 1 | ₹369 | https://robu.in/product/1240-12v-dc-0-6a-7-5w-solenoid-for-electric-door-lock/ |
| 7 | 1-channel 5V relay module (high-level trigger) | 1 | ₹69 | https://robu.in/product/1-channel-5v-relay-module-high-level/ |
| 8 | 1N4007 diode | 1 | ₹1 | https://robu.in/product/1n4007-through-hole-diodepack-of-5000/ |
| 9 | TowerPro MG996R servo (180°) | 1 | ₹409 | https://robu.in/product/towardpro-mg996r-digital-high-torque-servo-motor/ |
| 10 | HC-SR04 ultrasonic sensor | 2 | ₹79 | https://robu.in/product/hc-sr04-ultrasonic-range-finder/ |
| 11 | LCD1602 with I2C interface | 1 | ₹189 | https://robu.in/product/lcd1602-parallel-lcd-display-with-iic-i2c-interface/ |
| 12 | 5V active buzzer | 1 | ₹18 | https://robu.in/product/5v-active-electromagnetic-buzzer-pack-of-5/ |
| 13 | 1 kΩ resistor (through-hole) | 2 | ₹2.45 | https://robu.in/?s=1k+ohm+0.25w+carbon+film+resistor&post_type=product |
| 14 | 2 kΩ / 2.2 kΩ resistor (through-hole) | 2 | – | https://robu.in/?s=2.2k+ohm+0.25w+resistor&post_type=product |
| 15 | 1000 µF 16V/25V electrolytic capacitor | 1 | – | https://robu.in/?s=1000uf+25v+capacitor&post_type=product |
| 16 | 12V 2A power adapter (5.5 mm plug) | 1 | ₹399 | https://robu.in/product/orange-12v-2a-power-supply-with-5-5mm-dc-plug-adapter/ |
| 17 | 5.5 mm female DC jack | 1 | – | https://robu.in/?s=dc+barrel+jack+female+5.5&post_type=product |
| 18 | LM2596 buck converter with voltmeter display | 1 | ₹129 | https://robu.in/product/lm2596-buck-step-power-converter-module-dc-4-040-1-3-37v-led-voltmeter/ |
| 19 | 830-point breadboard (transparent) | 1 | ₹159 | https://robu.in/product/transparent-830-points-solderless-breadboard/ |
| 20 | Jumper wires M-M 40 pcs | 1 | ₹49 | https://robu.in/product/male-to-male-jumper-wires-40-pcs-10cm/ |
| 21 | Jumper wires M-F 40 pcs | 1 | ₹49 | https://robu.in/product/male-to-female-jumper-wires-40pcs-20cm/ |
| 22 | Jumper wires F-F 40 pcs | 1 | ₹49 | https://robu.in/product/10cm-female-female-breadboard-jumper-dupont-2-54mm-1p-1p-cable-40-pcs/ |

**Approximate total (linked items with prices): about ₹4,100.** Dustbins, flap and enclosure are extra; get these locally.

**Notes**
- **ESP32-CAM:** the complete ESP32-CAM kit is out of stock, so buy the board (2a) and the camera (2b) separately. Push the camera's ribbon cable into the board's connector and close the latch. The code doesn't change. When checked, the board was in stock but the OV2640 camera on Robu was **out of stock**. For the camera, get any **"OV2640 camera for ESP32-CAM" with a 24-pin ribbon cable** from any store. An **OV3660** camera with the same 24-pin ribbon also works with the same code.
- **Relay:** use a **high-level trigger** relay, because the code switches it on with HIGH. For a low-level relay, change `SOLENOID_ON` to `LOW` in `EcoStation_Main.ino`.
- **LM2596:** the version with the voltmeter display makes setting exactly 5.0V easy, with no multimeter needed.

## 6. Where to buy: Amazon.in (all in stock, 29 Sep 2026)

All of these were in stock and added to the Amazon cart. Unlike Robu, Amazon has the **complete ESP32-CAM with OV2640 camera**, so no separate camera is needed.

| # | Component | Qty | Price | Link |
|---|---|---|---|---|
| 1 | ESP32 DevKit V1 (30-pin, WROOM-32) | 1 | ₹559 | https://www.amazon.in/dp/B0HD7QC26L |
| 2 | ESP32-CAM with OV2640 camera (BITSKY) | 1 | ₹999 | https://www.amazon.in/dp/B0BXGWCL91 |
| 3 | ESP32-CAM-MB programmer (Roboduino) | 1 | ₹229 | https://www.amazon.in/dp/B0GGDDXNQR |
| 4 | RC522 RFID kit + card + key tag (ApTechDeals) | 1 | ₹157 | https://www.amazon.in/dp/B07Q1B6QZR |
| 5 | RFID cards 13.56 MHz, pack of 10 | 1 | ₹325 | https://www.amazon.in/dp/B0CL9YVX2V |
| 6 | Robodo 12V solenoid lock | 1 | ₹399 | https://www.amazon.in/dp/B07B918RK9 |
| 7 | 1-channel 5V relay, high/low trigger jumper (Electronic Spices) | 1 | ₹119 | https://www.amazon.in/dp/B08242LQ68 |
| 8 | 1N4007 diodes, pack of 50 | 1 | ₹94 | https://www.amazon.in/dp/B0BGX995MB |
| 9 | MG996R servo (Robocraze) | 1 | ₹395 | https://www.amazon.in/dp/B08F79X9CN |
| 10 | HC-SR04 ultrasonic sensor (Robocraze) | 2 | ₹170 | https://www.amazon.in/dp/B01I1ZTPJC |
| 11 | 16×2 LCD with pre-soldered I2C | 1 | ₹249 | https://www.amazon.in/dp/B0DQVF3HXG |
| 12 | 5V active buzzer | 1 | ₹55 | https://www.amazon.in/dp/B0H1X539R9 |
| 13 | Resistor kit, 500 pcs, 50 values | 1 | ₹189 | https://www.amazon.in/dp/B0DV93JVVK |
| 14 | 1000 µF 25V capacitors, pack of 10 | 1 | ₹134 | https://www.amazon.in/dp/B0H2WZBVCZ |
| 15 | ELOVE 12V 2A adapter, 5.5 mm jack | 1 | ₹399 | https://www.amazon.in/dp/B0D9H9QWVN |
| 16 | 5.5 mm DC jack female, pack of 10 | 1 | ₹127 | https://www.amazon.in/dp/B0BKQ1RTR3 |
| 17 | LM2596 buck converter with voltmeter display | 1 | ₹299 | https://www.amazon.in/dp/B0FBLFX15X |
| 18 | 830-point breadboard (Themisto) | 1 | ₹157 | https://www.amazon.in/dp/B0CGJDY5HL |
| 19 | Jumper wire set (M-F + M-M + F-F) | 1 | ₹299 | https://www.amazon.in/dp/B0G3XYG92V |

**Total: about ₹5,524** (₹5,643 if the relay stays at quantity 2).

**Notes**
- **Relay:** it has a jumper to choose High or Low trigger. Set it to **High (H)** to match the code.
- **Resistor kit:** it covers 1 Ω–2.2 MΩ in 50 values, but the listing doesn't list them. Make sure it includes 1 kΩ and 2 kΩ or 2.2 kΩ before you order.

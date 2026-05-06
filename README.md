# Spooky

A customized fork of nRFBox firmware for ESP32 with extended attack and research capabilities.

## What's New
- **TV-B-Gone** — IR power-off codes for TVs. Automatically starts after 5 seconds of device idle. Supported TVs: Sony, Samsung, Impex, Philips
- **BLE Spoofer** — Extended with new device profiles and added valid Fast Pair ID and Swift Pair
- **HID Injection** — Keyboard injection support added

- Note: For hid injection you have to add Instagram, YouTube videos in blutooth.cpp file.
line number 924 of bluetooth.cpp is for YouTube video
line number 955 of bluetooth.cpp is for Instagram reel (you can modify as your need)

## Hardware
- ESP32-WROOM-32
- SSD1306 OLED Display
- Push Buttons
- IR LED (Transmitter)
- 100 Ohm Resistor
- nRF24 Module x3

## IR LED Connection
- IR LED (+) to GPIO12 of ESP32 (use 100 Ohm resistor for protection)
- IR LED (−) to GND of ESP32

## Compilation Steps

### 1. Install ESP32 Board Support
Open **File → Preferences** and add to Additional Boards Manager URLs:

https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Go to **Tools → Board → Boards Manager**, search `esp32` by Espressif and install.

### 2. Board Settings
- Board: **ESP32 Dev Module**
- Partition Scheme: **Huge APP (3MB No OTA / 1MB SPIFFS)**

### 3. Install Libraries
Via **Tools → Manage Libraries**:
- `U8g2` by olikraus
- `Adafruit NeoPixel` by Adafruit
- `IRremote` by Shirriff/z3t0 (v4.x)
- `RF24` by TMRh20

Via **Sketch → Include Library → Add .ZIP Library**:
- `ESP32-BLE-Keyboard` by T-vK — [Download](https://github.com/T-vK/ESP32-BLE-Keyboard)

### 4. Upload
Select the correct port under **Tools → Port** and click **Upload**.

## Contact
- Email: febin.exe07@gmail.com
- Instagram: [@febin.exe07](https://instagram.com/febin.exe07)
- Telegram: [@Spooky_exe07](https://t.me/Spooky_exe07)

## Credits
Based on nRFBox by original authors.
https://github.com/cifertech/nrfbox

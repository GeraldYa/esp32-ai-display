# ESP32 AI Display Project

## Quick Start
ESP32-S3 smart dashboard with touchscreen and WiFi. **OTA is active** — push code wirelessly.

### Compile & Upload (OTA over WiFi)
```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB \
  --output-dir /tmp/esp32-build \
  /path/to/sketch

python3 ~/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py \
  -i 192.168.2.132 -p 3232 \
  -f /tmp/esp32-build/esp32-ai-display.ino.bin
```

### CRITICAL: Every sketch MUST include OTA code
If you push code without OTA, the device goes offline and needs USB to recover.
```cpp
#include <WiFi.h>
#include <ArduinoOTA.h>

// In setup():
WiFi.begin(WIFI_SSID, WIFI_PASS);  // credentials in config.h
ArduinoOTA.setHostname("esp32-ai-display");
ArduinoOTA.begin();

// In loop():
ArduinoOTA.handle();
```

## Hardware
- **Board:** Freenove ESP32-S3-WROOM N16R8 (16MB Flash, 8MB PSRAM)
- **Display:** 3.5" 480x320, ST7796 driver, SPI
- **Touch:** FT5x06 capacitive (CTP), I2C address 0x38
- **WiFi:** credentials in `config.h` (not committed)
- **Speaker:** MAX98357 I2S — **removed** (GPIO 1/2/3 unused)

## Pin Mapping
| Function | GPIO |
|----------|------|
| TFT MOSI | 11 |
| TFT MISO | 13 |
| TFT SCLK | 12 |
| TFT CS | 10 |
| TFT DC | 9 |
| TFT RST | 14 |
| TFT BL (PWM) | 21 |
| CTP SDA | 7 |
| CTP SCL | 15 |
| CTP RST | 6 |
| CTP INT | 8 |

## Display Init (required sequence)
```cpp
tft.begin();
tft.invertDisplay(true);  // REQUIRED — colors are wrong without this
tft.setRotation(1);       // Landscape 480x320
```
- TFT_eSPI User_Setup.h must have `ST7796_DRIVER` and `USE_HSPI_PORT` (FSPI crashes on ESP32-S3)
- `TOUCH_CS -1` (touch is I2C, not SPI)

## Touch (FT5x06 CTP)
```cpp
Wire.begin(7, 15);  // SDA=7, SCL=15
// Read from I2C address 0x38, register 0x02
// Landscape coordinate mapping (rotation 1):
int screenX = rawTouchY;
int screenY = 319 - rawTouchX;
```

## Server
- Port 3100, requires `server/.env` with `GEMINI_API_KEY`
- `cd server && node index.js`

## Libraries Required
- TFT_eSPI (with custom User_Setup.h)
- U8g2_for_Adafruit_GFX (Chinese font rendering via TFT_GFX_Adapter bridge)
- ArduinoOTA (built-in)

## Project Structure
- `esp32-ai-display.ino` — Main firmware (UI, touch, data, OTA)
- `config.h` — WiFi/API credentials (template, fill in your own)
- `ui.h` / `weather.h` / `stocks.h` / `news.h` — Module headers
- `server/` — Node.js Express backend
- `test_screen/` — Color diagnostic test
- `test_blink/` — LED blink test

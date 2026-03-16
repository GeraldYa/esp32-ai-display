# ESP32 AI Display Project

## Quick Start
This is an ESP32-S3 handheld AI display with touchscreen, speaker, and WiFi. **OTA is active** — push code wirelessly.

### Compile & Upload (OTA over WiFi)
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi /path/to/sketch
arduino-cli upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi --port esp32-ai-display.local /path/to/sketch
```

### ⚠️ CRITICAL: Every sketch MUST include OTA code
If you push code without OTA, the device goes offline and needs USB to recover.
```cpp
#include <WiFi.h>
#include <ArduinoOTA.h>

// In setup():
WiFi.begin("work", "REDACTED_WIFI_PASS");
ArduinoOTA.setHostname("esp32-ai-display");
ArduinoOTA.begin();

// In loop():
ArduinoOTA.handle();
```

## Hardware
- **Board:** Freenove ESP32-S3-WROOM N16R8 (16MB Flash, 8MB PSRAM)
- **Display:** 3.5" 480x320, ST7796 driver, SPI
- **Touch:** FT5x06 capacitive (CTP), I2C address 0x38
- **Speaker:** MAX98357 I2S amplifier
- **WiFi:** SSID `work` / Password `REDACTED_WIFI_PASS`

## Pin Mapping
| Function | GPIO |
|----------|------|
| TFT MOSI | 11 |
| TFT MISO | 13 |
| TFT SCLK | 12 |
| TFT CS | 10 |
| TFT DC | 9 |
| TFT RST | 14 |
| TFT BL | 21 |
| CTP SDA | 7 |
| CTP SCL | 15 |
| CTP RST | 6 |
| CTP INT | 8 |
| I2S BCLK | 1 |
| I2S LRC | 2 |
| I2S DIN | 3 |

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

## Speaker (I2S)
- 44100Hz sample rate, 16-bit stereo
- Use `i2s_driver_install()` with `I2S_NUM_0`
- Keep volume/gain ≤ 0.2 to avoid distortion

## Libraries Required
- TFT_eSPI (with custom User_Setup.h — already configured)
- ESP8266SAM_ES (Spanish TTS, works but sounds robotic)
- ArduinoOTA (built-in)

## Project Structure
- `esp32-ai-display.ino` — Main app (clock, tabs, touch nav)
- `test_speaker/` — Current active sketch with OTA + speaker
- `test_screen/` — Color diagnostic test
- `test_blink/` — Basic LED blink test

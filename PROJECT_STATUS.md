# ESP32 AI Display - Project Status Report

> Last updated: 2026-03-17
> Author: 洁柔 (Gerald)
> GitHub: https://github.com/GeraldYa/esp32-ai-display

---

## 1. Project Overview

ESP32-S3 驱动的智能桌面信息显示器，3.5 寸触摸屏，通过 WiFi 拉取天气、股票、新闻数据并展示，支持中英文切换、AI 新闻摘要、动漫风格背景图轮换和低功耗模式。

### Hardware

| Component | Spec |
|-----------|------|
| MCU | Freenove ESP32-S3-WROOM **N16R8** (16MB Flash, 8MB PSRAM) |
| Display | 3.5" **ST7796** TFT, **480x320**, SPI, 18-bit color depth |
| Touch | **FT5x06** capacitive (CTP), I2C @ 0x38 |
| WiFi | 802.11 b/g/n, connected to `work` SSID |
| Speaker | MAX98357 I2S amplifier — **removed** (tested, not in use) |
| Power | USB-C (planning battery + 3D printed case) |

### GPIO Pin Map

| Function | GPIO | Function | GPIO |
|----------|------|----------|------|
| TFT MOSI | 11 | CTP SDA | 7 |
| TFT MISO | 13 | CTP SCL | 15 |
| TFT SCLK | 12 | CTP RST | 6 |
| TFT CS | 10 | CTP INT | 8 |
| TFT DC | 9 | I2S BCLK | 1 (unused) |
| TFT RST | 14 | I2S LRC | 2 (unused) |
| TFT BL (PWM) | 21 | I2S DIN | 3 (unused) |

---

## 2. Architecture

```
┌─────────────────────┐         WiFi          ┌───────────────────────────┐
│    ESP32-S3         │ ◄──── HTTP REST ────► │  Node.js Server (:3100)   │
│  esp32-ai-display   │                       │  server/index.js          │
│  .ino (firmware)    │                       │                           │
│                     │  GET /api/dashboard   │  ┌─ Open-Meteo (weather)  │
│  3 tab pages:       │  GET /api/news/:id    │  ├─ Yahoo Finance (stock) │
│  - Weather/Clock    │  GET /api/bg/*        │  ├─ Google News RSS       │
│  - Stocks           │  POST /api/memory     │  ├─ Gemini AI (summary)   │
│  - News (+ detail)  │                       │  └─ 77 JPEG backgrounds   │
│                     │                       │                           │
│  Touch + OTA        │                       │  GET /preview             │
└─────────────────────┘                       │  (Web preview 480x320)    │
                                              └───────────────────────────┘
```

### Data Flow

1. ESP32 boots → connects WiFi → syncs NTP → reports memory to server
2. Fetches `/api/dashboard` (aggregated weather + stocks + news with AI translations)
3. Each page requests its own JPEG background via `/api/bg/{weather|stocks|news}`
4. News detail: fetches `/api/news/:id` for per-article AI summary
5. Server caches all external API calls (weather 10min, stocks 1min, news 30min)

---

## 3. Feature Inventory

### Completed Features

| Feature | Status | Details |
|---------|--------|---------|
| Weather display | ✅ Done | Temperature, feels-like, humidity, wind, high/low, city, description |
| Stock tracker | ✅ Done | AAPL, NVDA, AMD, NASDAQ, S&P500, TSLA — 2-column layout with arrows |
| News headlines | ✅ Done | 5 articles from Google News RSS, touch to enter detail view |
| News detail view | ✅ Done | Title + source + AI-generated 350-400 char summary, back button |
| Chinese translation | ✅ Done | Gemini AI translates titles + summaries to Chinese (150-200 chars) |
| Global language toggle | ✅ Done | Status bar EN/ZH indicator, affects all pages + nav bar |
| Chinese font rendering | ✅ Done | U8g2 `wqy16_gb2312` (full GB2312 6763 chars, 308KB) via TFT_GFX_Adapter bridge |
| Background images | ✅ Done | 77 anime-style JPEGs across 7 categories, per-minute rotation |
| Clock | ✅ Done | HH:MM format with date, flicker-free sprite update over background |
| Power saving | ✅ Done | PWM backlight 70%, 5min auto sleep, CPU 160MHz, WiFi modem sleep |
| OTA updates | ✅ Done | ArduinoOTA on port 3232, with progress bar display |
| Memory reporting | ✅ Done | ESP32 POSTs heap/PSRAM/flash info to server at boot |
| Web preview | ✅ Done | /preview page mirrors 480x320 display, synced with ESP32 UI |
| Boot animation | ✅ Done | Logo + progress bar + WiFi/data status |

### Not Implemented / Removed

| Feature | Status | Reason |
|---------|--------|--------|
| Speaker / Audio | ❌ Removed | Hardware detached after testing, pins 1/2/3 now unused |
| AI Chat | ❌ Not started | `ai_chat.h` exists but `AI_ENABLED = false` |
| Scrolling | ❌ Not planned | All content fits within fixed screen area by design |
| Touch gestures (swipe) | ❌ Not planned | Using tap-only navigation |

### Planned / Future

| Feature | Priority | Notes |
|---------|----------|-------|
| Battery power | High | Researching LiPo options, need power consumption data |
| 3D printed case | High | For enclosure after battery integration |
| Push to GitHub | High | Latest changes (i18n, power saving, backgrounds) not yet pushed |

---

## 4. File Structure

```
esp32-ai-display/                          # Project root
├── esp32-ai-display.ino  (1077 lines)     # Main firmware — all UI, touch, data, OTA
├── config.h                               # API keys, WiFi, colors (sanitized for GitHub)
├── ui.h                                   # UI helper functions
├── weather.h                              # Weather data parsing
├── stocks.h                               # Stock data parsing
├── news.h                                 # News data parsing
├── ai_chat.h                              # AI chat (disabled)
├── speaker.h                              # Speaker control (deprecated)
├── CLAUDE.md                              # Dev guide (OTA warning, pin map, init sequence)
├── README.md                              # Public documentation
├── .gitignore                             # Excludes build/, *.bin, node_modules/, audio/
│
├── server/                                # Node.js backend
│   ├── index.js           (284 lines)     # Express server — all API endpoints
│   ├── preview.html       (367 lines)     # Web preview (mirrors ESP32 display)
│   ├── gen-backgrounds.js (170 lines)     # Gemini image generation script
│   ├── backgrounds/       (77 files)      # JPEG backgrounds (480x266, ~15-30KB each)
│   │   ├── weather_clear_1..6.jpg         # Clear sky theme (6 variants)
│   │   ├── weather_cloud_1..6.jpg         # Cloudy theme (6 variants)
│   │   ├── weather_rain_1..5.jpg          # Rain theme (5 variants)
│   │   ├── weather_snow_1..5.jpg          # Snow theme (5 variants)
│   │   ├── weather_storm_1..5.jpg         # Storm theme (5 variants)
│   │   ├── stocks_1..25.jpg               # Money/business theme (25 variants)
│   │   └── news_1..25.jpg                 # Reading/newspapers theme (25 variants)
│   ├── audio/                             # Audio files (not in use)
│   ├── package.json
│   └── node_modules/
│
├── test_audio/                            # Speaker test sketch (I2S WAV playback)
├── test_blink/                            # LED blink test
└── test_screen/                           # Color diagnostic test
```

---

## 5. Server API Reference

**Base URL:** `http://192.168.2.106:3100`

| Endpoint | Method | Description | Cache TTL |
|----------|--------|-------------|-----------|
| `/api/dashboard` | GET | Aggregated weather + stocks + news (with Chinese translations) | varies |
| `/api/weather` | GET | Weather only | 10 min |
| `/api/stocks` | GET | Stocks only | 1 min |
| `/api/news` | GET | News list with summaries | 30 min |
| `/api/news/:id` | GET | Single news article detail | from cache |
| `/api/bg/weather` | GET | Weather background JPEG (rotates by minute, varies by weather icon) | — |
| `/api/bg/stocks` | GET | Stocks background JPEG (rotates by minute) | — |
| `/api/bg/news` | GET | News background JPEG (rotates by minute) | — |
| `/api/memory` | POST | ESP32 reports memory info (JSON body) | — |
| `/api/memory` | GET | Query last ESP32 memory report | — |
| `/api/audio/:name` | GET | Serve audio file | — |
| `/api/ping` | GET | Health check | — |
| `/preview` | GET | Web preview page (480x320 simulation) | — |

### External API Dependencies

| Service | Purpose | Auth | Free Tier |
|---------|---------|------|-----------|
| Open-Meteo | Weather data (Niagara Falls, ON) | No key needed | Unlimited |
| Yahoo Finance | Stock quotes (6 symbols) | No key needed | Unofficial |
| Google News | RSS feed (Tech category, US) | No key needed | Unlimited |
| Gemini (`gemini-3.1-flash-lite-preview`) | News summary + translation + image generation | API key via TokenTrove | Rate limited |

---

## 6. Build & Deploy

### Compile

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB \
  --output-dir /tmp/esp32-build \
  /home/gerald/.claude/workspace/telegram-bot/esp32-ai-display/
```

- Partition: `app3M_fat9M_16MB` (3MB app, 9MB FAT)
- Flash usage: **49%** (1550KB / 3145KB)
- Biggest contributor: `u8g2_font_wqy16_t_gb2312` font (308KB)

### OTA Upload (wireless)

```bash
python3 ~/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py \
  -i 192.168.2.132 -p 3232 \
  -f /tmp/esp32-build/esp32-ai-display.ino.bin
```

- ESP32 IP: `192.168.2.132` / mDNS: `esp32-ai-display.local`
- OTA port: 3232

### Server

```bash
cd /home/gerald/.claude/workspace/telegram-bot/esp32-ai-display/server
nohup node index.js > /dev/null 2>&1 &
```

- Runs on port **3100**, binds to `0.0.0.0`
- Pre-fetches all data on startup

### Generate Backgrounds

```bash
cd server && node gen-backgrounds.js
```

- Generates 77 anime-style JPEG via Gemini image API
- Skips existing files, 3s delay between API calls
- Output: 480x266 px, JPEG quality 75

---

## 7. Key Technical Decisions

### TFT_eSPI → U8g2 Bridge

TFT_eSPI does **not** inherit from `Adafruit_GFX`, but U8g2_for_Adafruit_GFX requires it. Solution: a minimal wrapper class `TFT_GFX_Adapter` that inherits `Adafruit_GFX` and delegates `drawPixel()` to `TFT_eSPI`:

```cpp
class TFT_GFX_Adapter : public Adafruit_GFX {
  TFT_GFX_Adapter(TFT_eSPI &t) : Adafruit_GFX(480, 320), _tft(t) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override { _tft.drawPixel(x, y, color); }
};
```

### Chinese Font Choice

Iterated through multiple fonts for coverage and readability:

| Font | Size | Coverage | Issue |
|------|------|----------|-------|
| `wqy12_chinese1` | 9KB | ~500 chars | Missing many common characters |
| `wqy12_chinese2` | 13KB | ~900 chars | Still missing chars like 癌疗患 |
| `wqy12_gb2312a` | 107KB | ~3000 chars | Readable but thin |
| `wqy12_gb2312` | 198KB | 6763 chars | Full GB2312 but 12px too thin |
| **`wqy16_gb2312`** | **308KB** | **6763 chars** | **Final choice — full GB2312, 16px, readable** |

### Background Rotation System

- File naming: `{category}_{n}.jpg` (e.g., `stocks_15.jpg`)
- Server scans directory at request time: `getBgVariants(prefix)`
- Selection: `minuteOfDay % variants.length` — changes every minute
- Weather backgrounds map to weather icon (clear/cloud/rain/snow/storm)

### Power Saving Stack

| Measure | Saving | Config |
|---------|--------|--------|
| PWM backlight 70% | ~30% backlight power | `BACKLIGHT_DUTY = 180` |
| Screen auto-sleep 5min | Full backlight off when idle | `SLEEP_TIMEOUT_MS = 300000` |
| CPU 160MHz (vs 240MHz) | ~20% CPU power | `setCpuFrequencyMhz(160)` |
| WiFi modem sleep | ~50mA idle savings | `WiFi.setSleep(WIFI_PS_MIN_MODEM)` |
| Data refresh 10min | Fewer HTTP requests | `DATA_REFRESH_MS = 600000` |

### Display Init (critical)

```cpp
tft.begin();
tft.invertDisplay(true);   // REQUIRED — colors inverted without this
tft.setRotation(1);        // Landscape 480x320
```

- Must use `USE_HSPI_PORT` in TFT_eSPI User_Setup.h (FSPI crashes on ESP32-S3)
- Touch coordinate mapping (rotation 1): `screenX = rawTouchY`, `screenY = 319 - rawTouchX`

---

## 8. Known Issues & Limitations

1. **No scrolling** — all content must fit within the visible area; summary lengths are tuned to fill but not overflow
2. **Yahoo Finance unofficial API** — may break if Yahoo changes their endpoint
3. **Single commit on GitHub** — extensive changes (i18n, power saving, backgrounds, preview sync) are uncommitted
4. **Speaker pins unused** — GPIO 1/2/3 are free after speaker removal
5. **No persistent language preference** — `langZh` resets to English on reboot
6. **Clock flicker on page switch** — background buffer only maintained for clock screen

---

## 9. Development Timeline

| Date | Milestone |
|------|-----------|
| 2026-03-15 | Project created — basic 3-tab UI, weather/stocks/news, OTA, boot animation |
| 2026-03-15 | Speaker tested (I2S WAV playback confirmed working) |
| 2026-03-15 | Chinese font rendering implemented (U8g2 via TFT_GFX_Adapter bridge) |
| 2026-03-15 | Font upgraded through 5 iterations to full GB2312 (wqy16) |
| 2026-03-15 | AI news summaries (Gemini) + Chinese translations |
| 2026-03-15 | News detail view with word-wrapped bilingual display |
| 2026-03-15 | Speaker hardware removed |
| 2026-03-16 | Power saving suite: PWM backlight, auto sleep, CPU 160MHz, WiFi modem sleep |
| 2026-03-16 | Background image system: 77 AI-generated anime-style JPEGs with per-minute rotation |
| 2026-03-16 | Global language toggle (EN/ZH) across all screens |
| 2026-03-16 | Preview page synced with ESP32 (no seconds, detail view, language toggle) |
| 2026-03-16 | ESP32 memory reporting via WiFi |
| 2026-03-16 | GitHub repo created and initial push |

---

## 10. ESP32 Memory (Last Reported)

Query via: `GET http://192.168.2.106:3100/api/memory`

Typical values:
- **Heap free:** ~180KB / 390KB total
- **PSRAM free:** ~7.5MB / 8MB total
- **Flash used:** ~1550KB (49% of 3MB partition)
- **CPU:** 160MHz

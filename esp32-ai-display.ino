/*
 * ESP32 AI Display v3 - Smart Dashboard
 * Hardware: ESP32-S3 N16R8 + 3.5" ST7796 CTP Touch (480x320)
 * Author: 洁柔
 */

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>

// ============ Config ============
#define WIFI_SSID       "work"
#define WIFI_PASSWORD   "REDACTED_WIFI_PASS"
#define API_BASE        "http://192.168.2.106:3100"
#define API_DASHBOARD   API_BASE "/api/dashboard"
#define API_BG_WEATHER  API_BASE "/api/bg/weather"
#define API_BG_STOCKS   API_BASE "/api/bg/stocks"
#define API_BG_NEWS     API_BASE "/api/bg/news"

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -18000
#define DAYLIGHT_OFFSET_SEC 3600

#define DATA_REFRESH_MS     600000  // Refresh data every 10 min

// Power saving
#define BACKLIGHT_PIN       21
#define BACKLIGHT_CHANNEL   0
#define BACKLIGHT_FREQ      5000
#define BACKLIGHT_BITS      8
#define BACKLIGHT_DUTY      180     // 0-255, lower = dimmer (180 ≈ 70%)
#define SLEEP_TIMEOUT_MS    300000  // Screen off after 5 min no touch

// CTP Touch pins
#define CTP_SDA  7
#define CTP_SCL 15
#define CTP_RST  6
#define CTP_INT  8
#define CTP_ADDR 0x38

// ============ Color Palette (dark modern theme) ============
#define C_BG          tft.color565(15, 15, 25)
#define C_CARD        tft.color565(25, 28, 45)
#define C_CARD_LIGHT  tft.color565(35, 40, 60)
#define C_ACCENT      tft.color565(80, 160, 255)
#define C_ACCENT2     tft.color565(130, 100, 255)
#define C_GREEN       tft.color565(50, 205, 100)
#define C_RED         tft.color565(255, 75, 75)
#define C_ORANGE      tft.color565(255, 165, 50)
#define C_TEXT        tft.color565(230, 230, 245)
#define C_TEXT_DIM    tft.color565(120, 125, 155)
#define C_DIVIDER     tft.color565(45, 50, 70)
#define C_NAV_BG      tft.color565(12, 12, 20)
#define C_NAV_ACTIVE  tft.color565(80, 160, 255)

// ============ Objects ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite(&tft);

// Minimal Adafruit_GFX adapter for U8g2 Chinese font rendering
class TFT_GFX_Adapter : public Adafruit_GFX {
public:
  TFT_GFX_Adapter(TFT_eSPI &t) : Adafruit_GFX(480, 320), _tft(t) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override { _tft.drawPixel(x, y, color); }
private:
  TFT_eSPI &_tft;
};

TFT_GFX_Adapter gfxAdapter(tft);
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// ============ Data Structures ============
struct WeatherData {
  char city[20];
  int temp;
  int feels_like;
  int humidity;
  char description[30];
  char icon[10];
  int wind;
  int high;
  int low;
} weather;

struct StockData {
  char symbol[8];
  float price;
  float change;
  float pct;
} stocks[6];
int stockCount = 0;

struct NewsData {
  char title[100];
  char source[30];
  char title_zh[200];   // UTF-8 Chinese title
  char summary_zh[800]; // UTF-8 Chinese summary
} news[5];
int newsCount = 0;

// ============ State ============
enum Screen { SCREEN_CLOCK, SCREEN_STOCKS, SCREEN_NEWS, SCREEN_NEWS_DETAIL };
const int NAV_TAB_COUNT = 3;  // Only first 3 are nav tabs
Screen currentScreen = SCREEN_CLOCK;
bool screenNeedsRedraw = true;
bool langZh = false;  // false=English, true=Chinese
int newsDetailIdx = -1;
char newsDetailSummary[800] = "";
char newsDetailSummaryZh[800] = "";
bool wifiConnected = false;
bool dataLoaded = false;
unsigned long lastDataFetch = 0;
unsigned long lastClockDraw = 0;
unsigned long lastTouchTime = 0;
bool screenAsleep = false;
int lastMinute = -1;
int lastSecond = -1;

// Clock background buffer (save bg pixels under clock area for flicker-free updates)
#define CLOCK_X 90
#define CLOCK_Y 30
#define CLOCK_W 300
#define CLOCK_H 80
uint16_t *clockBgBuf = nullptr;

// ============ Touch ============
struct TouchPoint { uint16_t x, y; bool pressed; };

TouchPoint readTouch() {
  TouchPoint tp = {0, 0, false};
  Wire.beginTransmission(CTP_ADDR);
  Wire.write(0x02);
  Wire.endTransmission();
  Wire.requestFrom(CTP_ADDR, (uint8_t)5);
  if (Wire.available() >= 5) {
    uint8_t touches = Wire.read() & 0x0F;
    if (touches > 0 && touches < 6) {
      uint8_t xH = Wire.read();
      uint8_t xL = Wire.read();
      uint8_t yH = Wire.read();
      uint8_t yL = Wire.read();
      tp.x = ((xH & 0x0F) << 8) | xL;
      tp.y = ((yH & 0x0F) << 8) | yL;
      tp.pressed = true;
    }
  }
  return tp;
}

// ============ JPEG Background ============
// TJpg_Decoder callback: draw decoded MCU blocks to TFT
bool jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= 320) return false;
  tft.pushImage(x, y + 18, w, h, bitmap);  // Offset by status bar height
  return true;
}

// Download JPEG from server and draw to screen
bool drawJpgBackground(const char* url) {
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  int code = http.GET();

  if (code != 200) {
    Serial.printf("[BG] HTTP error: %d\n", code);
    http.end();
    return false;
  }

  int len = http.getSize();
  if (len <= 0 || len > 100000) {
    Serial.printf("[BG] Bad size: %d\n", len);
    http.end();
    return false;
  }

  // Read JPEG into PSRAM buffer
  uint8_t *buf = (uint8_t *)ps_malloc(len);
  if (!buf) {
    Serial.println("[BG] PSRAM alloc failed");
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  int bytesRead = 0;
  while (bytesRead < len) {
    int available = stream->available();
    if (available) {
      int toRead = min(available, len - bytesRead);
      stream->readBytes(buf + bytesRead, toRead);
      bytesRead += toRead;
    }
    delay(1);
  }
  http.end();

  // Decode and draw
  TJpgDec.drawJpg(0, 0, buf, len);
  free(buf);
  Serial.printf("[BG] Drawn %d bytes\n", len);
  return true;
}

// ============ Network ============
bool fetchDashboard() {
  if (!wifiConnected) return false;

  HTTPClient http;
  http.begin(API_DASHBOARD);
  http.setTimeout(5000);
  int code = http.GET();

  if (code != 200) {
    Serial.printf("[API] Failed: %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[JSON] Error: %s\n", err.c_str());
    return false;
  }

  // Parse weather
  JsonObject w = doc["weather"];
  strlcpy(weather.city, w["city"] | "N/A", sizeof(weather.city));
  weather.temp = w["temp"] | 0;
  weather.feels_like = w["feels_like"] | 0;
  weather.humidity = w["humidity"] | 0;
  strlcpy(weather.description, w["description"] | "N/A", sizeof(weather.description));
  strlcpy(weather.icon, w["icon"] | "cloud", sizeof(weather.icon));
  weather.wind = w["wind"] | 0;
  weather.high = w["high"] | 0;
  weather.low = w["low"] | 0;

  // Parse stocks
  JsonArray sa = doc["stocks"];
  stockCount = min((int)sa.size(), 6);
  for (int i = 0; i < stockCount; i++) {
    strlcpy(stocks[i].symbol, sa[i]["symbol"] | "?", sizeof(stocks[i].symbol));
    stocks[i].price = sa[i]["price"] | 0.0f;
    stocks[i].change = sa[i]["change"] | 0.0f;
    stocks[i].pct = sa[i]["pct"] | 0.0f;
  }

  // Parse news
  JsonArray na = doc["news"];
  newsCount = min((int)na.size(), 5);
  for (int i = 0; i < newsCount; i++) {
    strlcpy(news[i].title, na[i]["title"] | "?", sizeof(news[i].title));
    strlcpy(news[i].source, na[i]["source"] | "?", sizeof(news[i].source));
    strlcpy(news[i].title_zh, na[i]["title_zh"] | "", sizeof(news[i].title_zh));
    strlcpy(news[i].summary_zh, na[i]["summary_zh"] | "", sizeof(news[i].summary_zh));
  }

  Serial.println("[API] Dashboard data loaded");
  dataLoaded = true;
  return true;
}

// ============ UI Helpers ============

// Draw gradient background (top to bottom, 2 colors)
void drawGradientBg(uint8_t r1, uint8_t g1, uint8_t b1,
                    uint8_t r2, uint8_t g2, uint8_t b2, int yStart, int yEnd) {
  for (int y = yStart; y < yEnd; y++) {
    float t = (float)(y - yStart) / (yEnd - yStart);
    uint8_t r = r1 + t * (r2 - r1);
    uint8_t g = g1 + t * (g2 - g1);
    uint8_t b = b1 + t * (b2 - b1);
    tft.drawFastHLine(0, y, 480, tft.color565(r, g, b));
  }
}

// Transparent key color for sprites (near-black but != TFT_BLACK)
#define C_TRANSPARENT tft.color565(8, 4, 8)

// Text with black outline for readability on any background
void drawOutlinedText(int x, int y, const char* text, uint16_t color) {
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(x - 1, y); tft.print(text);
  tft.setCursor(x + 1, y); tft.print(text);
  tft.setCursor(x, y - 1); tft.print(text);
  tft.setCursor(x, y + 1); tft.print(text);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

// Same but draws on sprite instead of tft
void spriteOutlinedText(TFT_eSprite &spr, int x, int y, const char* text, uint16_t color) {
  spr.setTextColor(TFT_BLACK);
  spr.setCursor(x - 1, y); spr.print(text);
  spr.setCursor(x + 1, y); spr.print(text);
  spr.setCursor(x, y - 1); spr.print(text);
  spr.setCursor(x, y + 1); spr.print(text);
  spr.setTextColor(color);
  spr.setCursor(x, y);
  spr.print(text);
}

// Draw Chinese (UTF-8) text with outline using U8g2
void drawOutlinedUTF8(int x, int y, const char* text, uint16_t color) {
  u8g2Fonts.setForegroundColor(TFT_BLACK);
  u8g2Fonts.drawUTF8(x - 1, y, text);
  u8g2Fonts.drawUTF8(x + 1, y, text);
  u8g2Fonts.drawUTF8(x, y - 1, text);
  u8g2Fonts.drawUTF8(x, y + 1, text);
  u8g2Fonts.setForegroundColor(color);
  u8g2Fonts.drawUTF8(x, y, text);
}

// Word-wrap UTF-8 Chinese text (break by characters, not words)
void drawWrappedUTF8(int x, int y, int maxW, int lineH, const char* text, uint16_t color) {
  char line[200] = "";
  int lineLen = 0;
  const char *p = text;

  while (*p) {
    // Determine UTF-8 char length
    int charLen = 1;
    if ((*p & 0xE0) == 0xC0) charLen = 2;
    else if ((*p & 0xF0) == 0xE0) charLen = 3;
    else if ((*p & 0xF8) == 0xF0) charLen = 4;

    // Try adding this char
    char test[200];
    memcpy(test, line, lineLen);
    memcpy(test + lineLen, p, charLen);
    test[lineLen + charLen] = '\0';

    if (u8g2Fonts.getUTF8Width(test) > maxW && lineLen > 0) {
      line[lineLen] = '\0';
      drawOutlinedUTF8(x, y, line, color);
      y += lineH;
      lineLen = 0;
    }

    memcpy(line + lineLen, p, charLen);
    lineLen += charLen;
    p += charLen;
  }

  if (lineLen > 0) {
    line[lineLen] = '\0';
    drawOutlinedUTF8(x, y, line, color);
  }
}

// Draw stock arrow (up/down triangle)
void drawArrow(int x, int y, bool up, uint16_t color) {
  if (up) {
    tft.fillTriangle(x, y - 5, x - 4, y + 3, x + 4, y + 3, color);
  } else {
    tft.fillTriangle(x, y + 5, x - 4, y - 3, x + 4, y - 3, color);
  }
}

// ============ Status Bar ============
void drawStatusBar() {
  // Semi-transparent status bar at top
  tft.fillRect(0, 0, 480, 18, tft.color565(10, 10, 18));
  tft.setFreeFont(NULL);
  tft.setTextColor(C_TEXT_DIM);
  tft.setTextSize(1);

  // WiFi status
  if (wifiConnected) {
    int rssi = WiFi.RSSI();
    // Signal bars
    int bars = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
      int bh = 3 + i * 2;
      uint16_t col = (i < bars) ? C_ACCENT : tft.color565(40, 40, 60);
      tft.fillRect(6 + i * 5, 14 - bh, 3, bh, col);
    }
    tft.setCursor(30, 5);
    tft.print(WiFi.localIP().toString());
  } else {
    tft.setCursor(6, 5);
    tft.print("No WiFi");
  }

  // Time in status bar
  struct tm ti;
  if (getLocalTime(&ti)) {
    char buf[6];
    sprintf(buf, "%02d:%02d", ti.tm_hour, ti.tm_min);
    tft.setCursor(430, 5);
    tft.print(buf);
  }

  // Language indicator (tap area 400-430, y<18)
  tft.setTextColor(C_ACCENT);
  tft.setCursor(405, 5);
  tft.print(langZh ? "ZH" : "EN");

  // Data status indicator
  if (dataLoaded) {
    tft.fillCircle(470, 9, 3, C_GREEN);
  } else {
    tft.fillCircle(470, 9, 3, C_RED);
  }
}

// ============ Nav Bar ============
void drawNavBar() {
  int y = 284;
  tft.fillRect(0, y, 480, 36, C_NAV_BG);
  tft.drawFastHLine(0, y, 480, C_DIVIDER);

  tft.setFreeFont(NULL);  // Reset font before drawing nav
  const char* labels[] = {
    langZh ? "\xe5\xa4\xa9\xe6\xb0\x94" : "Weather",   // 天气
    langZh ? "\xe8\x82\xa1\xe7\xa5\xa8" : "Stocks",     // 股票
    langZh ? "\xe6\x96\xb0\xe9\x97\xbb" : "News"        // 新闻
  };
  int tabW = 480 / NAV_TAB_COUNT;

  for (int i = 0; i < NAV_TAB_COUNT; i++) {
    int x = i * tabW;
    bool active = (i == currentScreen);
    uint16_t textCol = active ? C_NAV_ACTIVE : C_TEXT_DIM;

    if (active) {
      // Active indicator line
      tft.fillRect(x + tabW / 2 - 20, y + 1, 40, 2, C_NAV_ACTIVE);
    }

    // Icon dot
    int iconX = x + tabW / 2;
    if (active) {
      tft.fillCircle(iconX, y + 12, 3, C_NAV_ACTIVE);
    } else {
      tft.drawCircle(iconX, y + 12, 3, C_TEXT_DIM);
    }

    // Label
    if (langZh) {
      u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      u8g2Fonts.setFontMode(1);
      int tw = u8g2Fonts.getUTF8Width(labels[i]);
      u8g2Fonts.setForegroundColor(textCol);
      u8g2Fonts.drawUTF8(iconX - tw / 2, y + 26, labels[i]);
    } else {
      tft.setTextColor(textCol);
      tft.setTextSize(1);
      int tw = strlen(labels[i]) * 6;
      tft.setCursor(iconX - tw / 2, y + 22);
      tft.print(labels[i]);
    }
  }
}

// ============ Clock / Weather Screen ============
void drawClockScreen() {
  if (!drawJpgBackground(API_BG_WEATHER)) {
    drawGradientBg(10, 15, 40, 20, 10, 35, 18, 284);
  }

  // Save clock area background pixels to PSRAM for flicker-free updates
  if (!clockBgBuf) clockBgBuf = (uint16_t *)ps_malloc(CLOCK_W * CLOCK_H * 2);
  if (clockBgBuf) {
    tft.readRect(CLOCK_X, CLOCK_Y, CLOCK_W, CLOCK_H, clockBgBuf);
  }

  // Draw static weather info directly on background
  if (dataLoaded) {
    // Temperature (bottom-left)
    tft.setFreeFont(&FreeSansBold24pt7b);
    char tempStr[10];
    sprintf(tempStr, "%d°C", weather.temp);
    drawOutlinedText(20, 260, tempStr, C_TEXT);

    // Description
    tft.setFreeFont(&FreeSans9pt7b);
    drawOutlinedText(20, 278, weather.description, C_TEXT_DIM);

    // Details (bottom-right)
    tft.setFreeFont(&FreeSans9pt7b);
    char buf[48];
    if (langZh) {
      sprintf(buf, "%d°C", weather.feels_like);
      drawOutlinedText(300, 230, buf, C_TEXT);
      u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      u8g2Fonts.setFontMode(1);
      drawOutlinedUTF8(340, 230, "\xe4\xbd\x93\xe6\x84\x9f", C_TEXT);  // 体感
      sprintf(buf, "%d° / %d°", weather.high, weather.low);
      drawOutlinedText(300, 248, buf, C_TEXT);
      sprintf(buf, "\xe6\xb9\xbf\xe5\xba\xa6%d%%  \xe9\xa3\x8e%dkm/h", weather.humidity, weather.wind);
      u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      drawOutlinedUTF8(300, 266, buf, C_TEXT_DIM);  // 湿度/风
    } else {
      sprintf(buf, "Feels %d°C", weather.feels_like);
      drawOutlinedText(300, 230, buf, C_TEXT);
      sprintf(buf, "H %d° / L %d°", weather.high, weather.low);
      drawOutlinedText(300, 248, buf, C_TEXT);
      sprintf(buf, "Humid %d%%  Wind %d km/h", weather.humidity, weather.wind);
      drawOutlinedText(300, 266, buf, C_TEXT_DIM);
    }

    // City
    sprintf(buf, "@ %s", weather.city);
    drawOutlinedText(170, 278, buf, C_TEXT_DIM);
  }
}

void drawClockContent() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;

  bool minuteChanged = (ti.tm_min != lastMinute);
  if (!minuteChanged) return;

  // Small sprite with transparent background
  int sw = 300, sh = 80;
  sprite.createSprite(sw, sh);
  sprite.fillSprite(C_TRANSPARENT);

  // Time (hours:minutes only)
  sprite.setFreeFont(&FreeSansBold24pt7b);
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", ti.tm_hour, ti.tm_min);
  int tw = sprite.textWidth(timeStr);
  int tx = (sw - tw) / 2;
  spriteOutlinedText(sprite, tx, 42, timeStr, TFT_WHITE);

  // Date
  sprite.setFreeFont(&FreeSans12pt7b);
  char dateStr[32];
  strftime(dateStr, sizeof(dateStr), "%A, %B %d", &ti);
  int dw = sprite.textWidth(dateStr);
  spriteOutlinedText(sprite, (sw - dw) / 2, 70, dateStr, C_TEXT_DIM);

  // Restore background, then push text-only sprite on top
  if (clockBgBuf) {
    tft.pushImage(CLOCK_X, CLOCK_Y, CLOCK_W, CLOCK_H, clockBgBuf);
  }
  sprite.pushSprite(CLOCK_X, CLOCK_Y, C_TRANSPARENT);
  sprite.deleteSprite();

  lastMinute = ti.tm_min;
}

// ============ Stocks Screen ============
void drawStocksScreen() {
  if (!drawJpgBackground(API_BG_STOCKS)) {
    drawGradientBg(12, 18, 30, 18, 12, 28, 18, 284);
  }

  // Title
  if (langZh) {
    u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2Fonts.setFontMode(1);
    drawOutlinedUTF8(20, 45, "\xe8\xa1\x8c\xe6\x83\x85", C_TEXT);  // 行情
  } else {
    tft.setFreeFont(&FreeSansBold12pt7b);
    drawOutlinedText(20, 45, "Market Watch", C_TEXT);
  }

  if (!dataLoaded) {
    if (langZh) {
      u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      u8g2Fonts.setFontMode(1);
      drawOutlinedUTF8(180, 160, "\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad...", C_TEXT_DIM);  // 加载中...
    } else {
      tft.setFreeFont(&FreeSans9pt7b);
      drawOutlinedText(180, 160, "Loading...", C_TEXT_DIM);
    }
    return;
  }

  // Stocks in 2 columns, text directly on background
  int startY = 65;
  int rowH = 42;

  for (int i = 0; i < stockCount && i < 6; i++) {
    int col = i % 2;
    int row = i / 2;
    int x = 20 + col * 235;
    int y = startY + row * rowH;

    bool positive = stocks[i].change >= 0;
    uint16_t changeColor = positive ? C_GREEN : C_RED;

    // Color accent dot
    tft.fillCircle(x, y + 4, 4, changeColor);

    // Symbol
    tft.setFreeFont(&FreeSansBold9pt7b);
    drawOutlinedText(x + 12, y + 8, stocks[i].symbol, C_TEXT);

    // Price
    tft.setFreeFont(&FreeSans9pt7b);
    char priceStr[16];
    sprintf(priceStr, "$%.2f", stocks[i].price);
    drawOutlinedText(x + 12, y + 28, priceStr, C_TEXT);

    // Change + percentage
    tft.setFreeFont(&FreeSans9pt7b);
    char changeStr[20];
    sprintf(changeStr, "%s%.2f (%.1f%%)",
            positive ? "+" : "", stocks[i].change, stocks[i].pct);
    drawOutlinedText(x + 100, y + 8, changeStr, changeColor);

    // Arrow
    drawArrow(x + 100, y + 25, positive, changeColor);
  }
}

// ============ News Screen ============
void drawNewsScreen() {
  if (!drawJpgBackground(API_BG_NEWS)) {
    drawGradientBg(15, 12, 30, 20, 15, 25, 18, 284);
  }

  // Title
  if (langZh) {
    u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2Fonts.setFontMode(1);
    drawOutlinedUTF8(20, 45, "\xe5\xa4\xb4\xe6\x9d\xa1", C_TEXT);  // 头条
  } else {
    tft.setFreeFont(&FreeSansBold12pt7b);
    drawOutlinedText(20, 45, "Headlines", C_TEXT);
  }

  // Language toggle button (top-right)
  tft.setFreeFont(&FreeSansBold9pt7b);
  if (langZh) {
    drawOutlinedText(420, 45, "EN", C_ACCENT);
  } else {
    u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2Fonts.setFontMode(1);
    drawOutlinedUTF8(420, 45, "\xe4\xb8\xad", C_ACCENT);  // "中"
  }

  if (!dataLoaded) {
    tft.setFreeFont(&FreeSans9pt7b);
    drawOutlinedText(180, 160, "Loading...", C_TEXT_DIM);
    return;
  }

  int startY = 62;
  int rowH = 42;

  for (int i = 0; i < newsCount && i < 5; i++) {
    int y = startY + i * rowH;

    // Number
    tft.setFreeFont(&FreeSansBold9pt7b);
    char numStr[4];
    sprintf(numStr, "%d", i + 1);
    drawOutlinedText(20, y + 16, numStr, C_ACCENT2);

    if (langZh && strlen(news[i].title_zh) > 0) {
      // Chinese title using U8g2 font
      u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      u8g2Fonts.setFontMode(1);
      // Truncate display by pixel width
      drawOutlinedUTF8(38, y + 16, news[i].title_zh, C_TEXT);
    } else {
      // English title (truncate to fit)
      tft.setFreeFont(&FreeSans9pt7b);
      char truncTitle[45];
      strncpy(truncTitle, news[i].title, 44);
      truncTitle[44] = '\0';
      if (strlen(news[i].title) > 44) {
        truncTitle[41] = '.';
        truncTitle[42] = '.';
        truncTitle[43] = '.';
      }
      drawOutlinedText(38, y + 16, truncTitle, C_TEXT);
    }

    // Source
    tft.setFreeFont(NULL);
    tft.setTextSize(1);
    drawOutlinedText(38, y + 28, news[i].source, C_ACCENT2);
  }
}

// ============ News Detail Screen ============
bool fetchNewsSummary(int idx) {
  if (!wifiConnected || idx < 0 || idx >= newsCount) return false;

  char url[64];
  sprintf(url, "%s/api/news/%d", API_BASE, idx);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();

  if (code != 200) {
    Serial.printf("[Detail] HTTP error: %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;

  const char *summary = doc["summary"] | "";
  strlcpy(newsDetailSummary, summary, sizeof(newsDetailSummary));
  const char *summaryZh = doc["summary_zh"] | "";
  strlcpy(newsDetailSummaryZh, summaryZh, sizeof(newsDetailSummaryZh));
  Serial.printf("[Detail] Got summary for #%d (%d chars)\n", idx, strlen(newsDetailSummary));
  return true;
}

// Word-wrap text and draw line by line
void drawWrappedText(int x, int y, int maxW, int lineH, const char* text, uint16_t color) {
  char word[50];
  char line[80] = "";
  int wi = 0;
  const char *p = text;

  while (*p) {
    if (*p == ' ' || *p == '\n' || *(p + 1) == '\0') {
      if (*(p + 1) == '\0' && *p != ' ' && *p != '\n') {
        word[wi++] = *p;
      }
      word[wi] = '\0';

      // Check if adding this word exceeds width
      char test[130];
      sprintf(test, "%s%s%s", line, strlen(line) > 0 ? " " : "", word);
      if (tft.textWidth(test) > maxW && strlen(line) > 0) {
        drawOutlinedText(x, y, line, color);
        y += lineH;
        strcpy(line, word);
      } else {
        strcpy(line, test);
      }
      wi = 0;
    } else {
      if (wi < 48) word[wi++] = *p;
    }
    p++;
  }
  if (strlen(line) > 0) {
    drawOutlinedText(x, y, line, color);
  }
}

void drawNewsDetailScreen() {
  if (!drawJpgBackground(API_BG_NEWS)) {
    drawGradientBg(15, 12, 30, 20, 15, 25, 18, 284);
  }

  if (newsDetailIdx < 0 || newsDetailIdx >= newsCount) return;

  // Back button (larger, visible)
  if (langZh) {
    u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2Fonts.setFontMode(1);
    drawOutlinedUTF8(15, 38, "< \xe8\xbf\x94\xe5\x9b\x9e", C_ACCENT);  // < 返回
  } else {
    tft.setFreeFont(&FreeSansBold9pt7b);
    drawOutlinedText(15, 38, "< Back", C_ACCENT);
  }

  if (langZh && strlen(news[newsDetailIdx].title_zh) > 0) {
    // Chinese detail view
    u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2Fonts.setFontMode(1);

    // Chinese title (word-wrapped, ~2 lines max)
    drawWrappedUTF8(20, 62, 440, 20, news[newsDetailIdx].title_zh, C_TEXT);

    // Source
    tft.setFreeFont(&FreeSans9pt7b);
    char srcLine[40];
    sprintf(srcLine, "- %s", news[newsDetailIdx].source);
    drawOutlinedText(20, 105, srcLine, C_ACCENT2);

    // Chinese summary (y=120 to y=275, ~8 lines at 20px)
    const char *zh = newsDetailSummaryZh;
    if (strlen(zh) == 0) zh = news[newsDetailIdx].summary_zh;
    if (strlen(zh) > 0) {
      u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      drawWrappedUTF8(20, 125, 440, 20, zh, C_TEXT_DIM);
    } else {
      tft.setFreeFont(&FreeSans9pt7b);
      drawOutlinedText(20, 140, "Loading...", C_TEXT_DIM);
    }
  } else {
    // English detail view
    tft.setFreeFont(&FreeSansBold9pt7b);
    drawWrappedText(20, 62, 440, 18, news[newsDetailIdx].title, C_TEXT);

    tft.setFreeFont(&FreeSans9pt7b);
    char srcLine[40];
    sprintf(srcLine, "- %s", news[newsDetailIdx].source);
    drawOutlinedText(20, 105, srcLine, C_ACCENT2);

    // English summary (y=120 to y=275, ~8 lines at 20px)
    if (strlen(newsDetailSummary) > 0) {
      tft.setFreeFont(&FreeSans9pt7b);
      drawWrappedText(20, 125, 440, 20, newsDetailSummary, C_TEXT_DIM);
    } else {
      tft.setFreeFont(&FreeSans9pt7b);
      drawOutlinedText(20, 140, "Loading summary...", C_TEXT_DIM);
    }
  }
}

// ============ Setup ============
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[AI Display v3] Starting...");

  // CPU frequency (160MHz saves ~20% power vs 240MHz)
  setCpuFrequencyMhz(160);

  // PWM backlight (70% brightness saves ~30% backlight power)
  ledcAttach(BACKLIGHT_PIN, BACKLIGHT_FREQ, BACKLIGHT_BITS);
  ledcWrite(BACKLIGHT_PIN, BACKLIGHT_DUTY);

  // Display init
  tft.begin();
  tft.invertDisplay(true);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // CTP Touch init
  pinMode(CTP_RST, OUTPUT);
  digitalWrite(CTP_RST, LOW);
  delay(50);
  digitalWrite(CTP_RST, HIGH);
  delay(200);
  pinMode(CTP_INT, INPUT);
  Wire.begin(CTP_SDA, CTP_SCL);

  // JPEG decoder setup
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);  // Fix RGB/BGR byte order for TFT
  TJpgDec.setCallback(jpgDrawCallback);

  // U8g2 Chinese font engine (via Adafruit_GFX adapter)
  u8g2Fonts.begin(gfxAdapter);

  // ---- Boot animation ----
  drawGradientBg(5, 5, 20, 15, 10, 35, 0, 320);

  // Logo area
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setTextColor(C_ACCENT);
  const char* title = "AI Display";
  int tw = tft.textWidth(title);
  tft.setCursor((480 - tw) / 2, 130);
  tft.print(title);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(185, 165);
  tft.print("by Jierou  v3.0");

  // Progress bar area
  int barX = 120, barY = 200, barW = 240, barH = 6;
  tft.drawRoundRect(barX, barY, barW, barH, 3, C_DIVIDER);

  // WiFi connect with progress
  tft.setTextColor(C_TEXT_DIM);
  tft.setFreeFont(NULL);
  tft.setTextSize(1);
  tft.setCursor(190, 220);
  tft.print("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-ai-display");
  WiFi.setSleep(WIFI_PS_MIN_MODEM);  // Light sleep - saves ~50mA while idle
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    int progress = map(attempts, 0, 20, 0, barW - 4);
    tft.fillRoundRect(barX + 2, barY + 1, progress, barH - 2, 2, C_ACCENT);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    tft.fillRoundRect(barX + 2, barY + 1, barW - 4, barH - 2, 2, C_GREEN);

    tft.fillRect(150, 215, 200, 15, tft.color565(10, 8, 30));
    tft.setTextColor(C_GREEN);
    tft.setCursor(170, 220);
    tft.printf("WiFi OK: %s", WiFi.localIP().toString().c_str());

    // NTP sync
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm ti;
    int syncAttempts = 0;
    while (!getLocalTime(&ti) && syncAttempts < 10) {
      delay(500);
      syncAttempts++;
    }

    // OTA setup
    ArduinoOTA.setHostname("esp32-ai-display");
    ArduinoOTA.setPort(3232);
    ArduinoOTA.onStart([]() {
      tft.fillScreen(TFT_BLACK);
      tft.setFreeFont(&FreeSans12pt7b);
      tft.setTextColor(C_ACCENT);
      tft.setCursor(150, 150);
      tft.print("OTA Updating...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      int pct = progress / (total / 100);
      tft.fillRect(120, 170, 240, 10, TFT_BLACK);
      tft.fillRoundRect(120, 170, pct * 2.4, 10, 3, C_ACCENT);
    });
    ArduinoOTA.onEnd([]() {
      tft.setCursor(160, 200);
      tft.setTextColor(C_GREEN);
      tft.print("Rebooting...");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("[OTA] Error[%u]\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");

    // Initial data fetch
    tft.fillRect(150, 215, 200, 15, tft.color565(10, 8, 30));
    tft.setTextColor(C_TEXT_DIM);
    tft.setCursor(175, 220);
    tft.print("Fetching data...");
    fetchDashboard();
  } else {
    tft.fillRoundRect(barX + 2, barY + 1, barW - 4, barH - 2, 2, C_RED);
    tft.fillRect(150, 215, 200, 15, tft.color565(10, 8, 30));
    tft.setTextColor(C_RED);
    tft.setCursor(185, 220);
    tft.print("WiFi Failed!");
  }

  delay(1000);
  tft.fillScreen(TFT_BLACK);
  screenNeedsRedraw = true;
}

// ============ Main Loop ============
void loop() {
  ArduinoOTA.handle();
  unsigned long now = millis();

  // Periodic data refresh
  if (wifiConnected && (now - lastDataFetch > DATA_REFRESH_MS || lastDataFetch == 0)) {
    fetchDashboard();
    lastDataFetch = now;
    if (currentScreen != SCREEN_CLOCK) screenNeedsRedraw = true;
  }

  // Screen sleep after timeout
  if (!screenAsleep && (now - lastTouchTime > SLEEP_TIMEOUT_MS) && lastTouchTime > 0) {
    ledcWrite(BACKLIGHT_PIN, 0);
    screenAsleep = true;
    Serial.println("[Power] Screen sleep");
  }

  // Touch handling
  TouchPoint tp = readTouch();
  if (tp.pressed && now - lastTouchTime > 300) {
    lastTouchTime = now;

    // Wake from sleep — consume this touch, don't act on it
    if (screenAsleep) {
      screenAsleep = false;
      ledcWrite(BACKLIGHT_PIN, BACKLIGHT_DUTY);
      Serial.println("[Power] Screen wake");
      goto skip_touch;
    }
    int sx = tp.y;
    int sy = 319 - tp.x;

    // Global language toggle (status bar area, x>400 y<18)
    if (sy < 18 && sx > 400) {
      langZh = !langZh;
      screenNeedsRedraw = true;
      lastMinute = -1;  // Force clock redraw
      Serial.printf("[Lang] Switched to %s\n", langZh ? "Chinese" : "English");
    }
    // Nav bar tap
    else if (sy > 284) {
      int tabW = 480 / NAV_TAB_COUNT;
      int tabIdx = sx / tabW;
      if (tabIdx >= 0 && tabIdx < NAV_TAB_COUNT) {
        Screen newScreen = (Screen)tabIdx;
        if (newScreen != currentScreen) {
          currentScreen = newScreen;
          screenNeedsRedraw = true;
          lastMinute = -1;
          lastSecond = -1;
        }
      }
    }
    // News list: tap on article to open detail
    else if (currentScreen == SCREEN_NEWS && sy >= 62 && sy < 62 + 5 * 42) {
      int idx = (sy - 62) / 42;
      if (idx >= 0 && idx < newsCount) {
        newsDetailIdx = idx;
        newsDetailSummary[0] = '\0';
        currentScreen = SCREEN_NEWS_DETAIL;
        screenNeedsRedraw = true;
        // Fetch summary in foreground (brief pause)
        fetchNewsSummary(idx);
        screenNeedsRedraw = true;  // Redraw with summary
      }
    }
    // News detail: tap "< Back" area to go back
    else if (currentScreen == SCREEN_NEWS_DETAIL && sy < 60) {
      currentScreen = SCREEN_NEWS;
      screenNeedsRedraw = true;
    }
  }
  skip_touch:

  // Redraw screen
  if (screenNeedsRedraw) {
    // Draw full screen - each screen paints its own background fully
    drawStatusBar();
    switch (currentScreen) {
      case SCREEN_CLOCK:       drawClockScreen(); drawClockContent(); break;
      case SCREEN_STOCKS:      drawStocksScreen(); break;
      case SCREEN_NEWS:        drawNewsScreen(); break;
      case SCREEN_NEWS_DETAIL: drawNewsDetailScreen(); break;
      default: break;
    }
    drawNavBar();
    screenNeedsRedraw = false;
  }

  // Clock screen real-time update
  if (currentScreen == SCREEN_CLOCK && now - lastClockDraw > 1000) {
    drawClockContent();
    lastClockDraw = now;
  }

  delay(20);
}

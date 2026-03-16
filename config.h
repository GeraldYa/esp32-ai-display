/*
 * config.h - 所有配置项集中管理
 *
 * ⚠️ 注意: 请修改下面的 WiFi 和 API Key 为你自己的
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============ WiFi ============
#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// ============ Time / NTP ============
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -18000    // Toronto = UTC-5 = -5*3600
#define DAYLIGHT_OFFSET_SEC 3600      // DST +1h (夏令时)

// ============ OpenWeatherMap (免费) ============
// 注册: https://openweathermap.org/api → Current Weather
#define WEATHER_API_KEY     "YOUR_OPENWEATHER_KEY"
#define WEATHER_CITY        "Toronto"
#define WEATHER_COUNTRY     "CA"
#define WEATHER_UNITS       "metric"  // Celsius
#define WEATHER_INTERVAL    600000    // 10分钟刷新一次 (ms)

// ============ Finnhub 股票 (免费) ============
// 注册: https://finnhub.io/ → Free tier 60 calls/min
#define STOCK_API_KEY       "YOUR_FINNHUB_KEY"
#define STOCK_INTERVAL      60000     // 1分钟刷新 (ms)

// 你要追踪的股票 (最多8个，屏幕限制)
#define STOCK_COUNT         6
const char* STOCK_SYMBOLS[STOCK_COUNT] = {
  "AAPL",   // Apple
  "NVDA",   // Nvidia
  "MSFT",   // Microsoft
  "GOOGL",  // Google
  "AMZN",   // Amazon
  "TSLA",   // Tesla
};

// ============ NewsAPI (免费) ============
// 注册: https://newsapi.org/ → Developer plan (100 req/day)
#define NEWS_API_KEY        "YOUR_NEWSAPI_KEY"
#define NEWS_CATEGORY       "technology"
#define NEWS_COUNTRY        "us"
#define NEWS_MAX_ARTICLES   5
#define NEWS_INTERVAL       1800000   // 30分钟刷新 (ms)

// ============ AI Chat (可选) ============
// 方案1: 用自建proxy中转Claude/OpenAI API
// 方案2: 直接调ESP32的HTTPS（内存紧张，建议proxy）
#define AI_ENABLED          false     // 先设false，其他功能跑通再开
#define AI_PROXY_URL        "http://YOUR_SERVER:8080/chat"  // 你的中转服务器
#define AI_MAX_TOKENS       150       // ESP32内存有限，回答要短

// ============ Speaker ============
#define SPEAKER_PIN         25        // DAC输出引脚，根据你的接线改
#define SPEAKER_ENABLED     true

// ============ Display Pins (TFT_eSPI在User_Setup.h中配置) ============
// ST7796 SPI pins - 需要在 TFT_eSPI 库的 User_Setup.h 中配置:
//   #define ST7796_DRIVER
//   #define TFT_WIDTH  320
//   #define TFT_HEIGHT 480
//   #define TFT_MISO   19
//   #define TFT_MOSI   23
//   #define TFT_SCLK   18
//   #define TFT_CS     15
//   #define TFT_DC     2
//   #define TFT_RST    4
//   #define TFT_BL     21  (背光)
//   #define TOUCH_CS   -1  (CTP不需要这个)
//
// CTP触控 (I2C):
//   SDA = 21
//   SCL = 22
//   INT = 36 (可选)

// ============ UI Colors (RGB565) ============
#define COLOR_BG          0x0000    // 黑色背景
#define COLOR_PRIMARY     0x07FF    // 青色
#define COLOR_SECONDARY   0xFD20    // 橙色
#define COLOR_POSITIVE    0x07E0    // 绿色 (股票涨)
#define COLOR_NEGATIVE    0xF800    // 红色 (股票跌)
#define COLOR_TEXT        0xFFFF    // 白色文字
#define COLOR_TEXT_DIM    0x7BEF    // 灰色文字
#define COLOR_DIVIDER     0x2104    // 分割线

#endif // CONFIG_H

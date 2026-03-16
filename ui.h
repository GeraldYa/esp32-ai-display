/*
 * ui.h - UI绘制模块
 * 负责各个屏幕的绘制和导航栏
 */

#ifndef UI_H
#define UI_H

#include <TFT_eSPI.h>
#include "config.h"

extern TFT_eSPI tft;
extern TFT_eSprite clockSprite;

// ============ Nav Bar ============
void drawNavBar(int activeScreen) {
  int barY = 280;
  int barH = 40;
  int btnW = 120;

  const char* labels[] = {"Clock", "Stock", "News", "AI"};
  const uint16_t activeColor = COLOR_PRIMARY;
  const uint16_t inactiveColor = COLOR_TEXT_DIM;

  // 底部背景
  tft.fillRect(0, barY, 480, barH, 0x1082);

  for (int i = 0; i < 4; i++) {
    int x = i * btnW;
    uint16_t color = (i == activeScreen) ? activeColor : inactiveColor;

    // 激活指示条
    if (i == activeScreen) {
      tft.fillRect(x + 10, barY, btnW - 20, 3, activeColor);
    }

    tft.setTextColor(color);
    tft.setTextSize(2);
    // 居中文字
    int textW = strlen(labels[i]) * 12;
    tft.setCursor(x + (btnW - textW) / 2, barY + 12);
    tft.print(labels[i]);
  }
}

// ============ Screen Router ============
void drawScreen(int screen) {
  // 清除内容区域 (保留nav bar)
  tft.fillRect(0, 0, 480, 280, COLOR_BG);

  switch (screen) {
    case 0: drawClockWeatherScreen(); break;
    case 1: drawStocksScreen();       break;
    case 2: drawNewsScreen();         break;
    case 3: drawAIChatScreen();       break;
  }
}

// ============ Clock + Weather Screen ============
void drawClockWeatherScreen() {
  // 时钟在 drawClockOnly() 中每秒更新
  drawClockOnly();

  // 日期
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char dateStr[32];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %A", &timeinfo);
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.setTextSize(1);
    tft.setCursor(160, 110);
    tft.print(dateStr);
  }

  // 分割线
  tft.drawFastHLine(20, 130, 440, COLOR_DIVIDER);

  // 天气区域
  drawWeatherWidget(20, 140);
}

// ============ Clock Sprite (每秒更新，不闪烁) ============
void drawClockOnly() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  clockSprite.fillSprite(COLOR_BG);
  clockSprite.setTextColor(COLOR_PRIMARY);
  clockSprite.setTextSize(5);
  clockSprite.setCursor(0, 10);
  clockSprite.print(timeStr);
  clockSprite.pushSprite(100, 30);
}

// ============ Weather Widget ============
void drawWeatherWidget(int x, int y) {
  extern float weatherTemp;
  extern float weatherHumidity;
  extern int weatherCode;
  extern char weatherDesc[64];
  extern bool weatherValid;

  if (!weatherValid) {
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.setTextSize(2);
    tft.setCursor(x, y);
    tft.print("Weather loading...");
    return;
  }

  // 温度 (大字)
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(4);
  tft.setCursor(x, y);
  tft.printf("%.0f", weatherTemp);
  tft.setTextSize(2);
  tft.print(" C");

  // 天气描述
  tft.setTextSize(2);
  tft.setCursor(x, y + 50);
  tft.setTextColor(COLOR_SECONDARY);
  tft.print(weatherDesc);

  // 湿度
  tft.setCursor(x, y + 80);
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextSize(1);
  tft.printf("Humidity: %.0f%%", weatherHumidity);

  // Toronto 标识
  tft.setCursor(x + 300, y);
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextSize(2);
  tft.print("Toronto");
}

// ============ Stocks Screen ============
void drawStocksScreen() {
  extern StockData stocks[STOCK_COUNT];
  extern bool stocksValid;

  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.print("US Stocks");

  tft.drawFastHLine(20, 35, 440, COLOR_DIVIDER);

  if (!stocksValid) {
    tft.setCursor(20, 60);
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.print("Loading...");
    return;
  }

  for (int i = 0; i < STOCK_COUNT; i++) {
    int y = 45 + i * 38;

    // Symbol
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(2);
    tft.setCursor(20, y);
    tft.print(stocks[i].symbol);

    // Price
    tft.setCursor(160, y);
    tft.printf("$%.2f", stocks[i].price);

    // Change %
    float change = stocks[i].changePercent;
    uint16_t changeColor = (change >= 0) ? COLOR_POSITIVE : COLOR_NEGATIVE;
    tft.setTextColor(changeColor);
    tft.setCursor(340, y);
    tft.printf("%s%.2f%%", (change >= 0) ? "+" : "", change);

    // 分割线
    if (i < STOCK_COUNT - 1) {
      tft.drawFastHLine(20, y + 30, 440, COLOR_DIVIDER);
    }
  }
}

void handleStocksTouch(uint16_t x, uint16_t y) {
  // 点击某只股票可以展示详情（后续扩展）
}

// ============ News Screen ============
void drawNewsScreen() {
  extern NewsItem newsItems[NEWS_MAX_ARTICLES];
  extern int newsCount;
  extern bool newsValid;
  extern int newsScrollOffset;

  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.print("Tech News");

  tft.drawFastHLine(20, 35, 440, COLOR_DIVIDER);

  if (!newsValid) {
    tft.setCursor(20, 60);
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.print("Loading...");
    return;
  }

  for (int i = 0; i < newsCount && i < 4; i++) {
    int idx = i + newsScrollOffset;
    if (idx >= newsCount) break;

    int y = 45 + i * 58;

    // Title (truncated to fit)
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    tft.setCursor(20, y);

    // 截断标题到~60字符
    char title[61];
    strncpy(title, newsItems[idx].title, 60);
    title[60] = '\0';
    tft.print(title);

    // Source + time
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.setCursor(20, y + 18);
    tft.print(newsItems[idx].source);

    if (i < 3) {
      tft.drawFastHLine(20, y + 48, 440, COLOR_DIVIDER);
    }
  }

  // 滚动提示
  if (newsCount > 4) {
    tft.setTextColor(COLOR_SECONDARY);
    tft.setTextSize(1);
    tft.setCursor(400, 265);
    tft.print("Swipe");
  }
}

void handleNewsTouch(uint16_t x, uint16_t y) {
  extern int newsScrollOffset;
  extern int newsCount;
  extern bool screenNeedsRedraw;

  // 简单翻页：点上半屏向上，下半屏向下
  if (y < 160 && newsScrollOffset > 0) {
    newsScrollOffset--;
    screenNeedsRedraw = true;
  } else if (y >= 160 && newsScrollOffset < newsCount - 4) {
    newsScrollOffset++;
    screenNeedsRedraw = true;
  }
}

// ============ AI Chat Screen ============
void drawAIChatScreen() {
  extern char aiResponse[512];
  extern bool aiWaiting;
  extern bool aiValid;

  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.print("AI Assistant");

  tft.drawFastHLine(20, 35, 440, COLOR_DIVIDER);

  if (!AI_ENABLED) {
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.setTextSize(1);
    tft.setCursor(20, 60);
    tft.println("AI chat disabled.");
    tft.setCursor(20, 80);
    tft.println("Set AI_ENABLED=true in config.h");
    tft.setCursor(20, 100);
    tft.println("and configure your proxy server.");
    return;
  }

  if (aiWaiting) {
    tft.setTextColor(COLOR_SECONDARY);
    tft.setTextSize(2);
    tft.setCursor(20, 120);
    tft.print("Thinking...");
    return;
  }

  // 显示AI回复
  if (aiValid) {
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    tft.setCursor(20, 45);
    // Word wrap for small screen
    printWrapped(aiResponse, 20, 45, 440, 230);
  }

  // 底部操作按钮
  tft.fillRoundRect(20, 250, 200, 25, 5, COLOR_PRIMARY);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(50, 256);
  tft.print("Ask: What's trending?");
}

void handleAIChatTouch(uint16_t x, uint16_t y) {
  extern bool screenNeedsRedraw;

  // 点击底部按钮发送预设问题
  if (y > 240 && x < 220) {
    sendAIQuery("What's the most interesting tech news today? Keep it under 100 words.");
    screenNeedsRedraw = true;
  }
}

// ============ Text Wrapping Helper ============
void printWrapped(const char* text, int x, int y, int maxW, int maxH) {
  int curX = x;
  int curY = y;
  int charW = 6;   // size 1 char width
  int lineH = 12;  // size 1 line height

  for (int i = 0; text[i] && curY < y + maxH; i++) {
    if (text[i] == '\n' || curX + charW > x + maxW) {
      curX = x;
      curY += lineH;
    }
    if (text[i] != '\n') {
      tft.setCursor(curX, curY);
      tft.print(text[i]);
      curX += charW;
    }
  }
}

#endif // UI_H

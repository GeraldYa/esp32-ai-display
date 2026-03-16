// 颜色诊断测试
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.begin();
  tft.invertDisplay(true);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(3);

  // 红色块 + 标签
  tft.fillRect(20, 20, 100, 60, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(140, 35);
  tft.println("RED?");

  // 绿色块 + 标签
  tft.fillRect(20, 100, 100, 60, TFT_GREEN);
  tft.setCursor(140, 115);
  tft.println("GREEN?");

  // 蓝色块 + 标签
  tft.fillRect(20, 180, 100, 60, TFT_BLUE);
  tft.setCursor(140, 195);
  tft.println("BLUE?");

  // 黄色块
  tft.fillRect(300, 20, 100, 60, TFT_YELLOW);
  tft.setCursor(300, 95);
  tft.setTextSize(2);
  tft.println("YELLOW?");

  Serial.println("Color test displayed. Tell me what colors you see!");
}

void loop() {
  delay(1000);
}

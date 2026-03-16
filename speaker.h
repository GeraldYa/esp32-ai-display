/*
 * speaker.h - 扬声器模块
 * 用 DAC 输出简单音效，后续可扩展 I2S + TTS
 */

#ifndef SPEAKER_H
#define SPEAKER_H

#include "config.h"

void speakerInit() {
  if (!SPEAKER_ENABLED) return;
  // ESP32 DAC 不需要特别初始化，直接写就行
  // 如果用 I2S 外接功放，这里改成 I2S 初始化
  Serial.println("[Speaker] Init OK (DAC mode)");
}

// 简单方波蜂鸣
void speakerBeep(int freqHz, int durationMs) {
  if (!SPEAKER_ENABLED) return;

  int halfPeriod = 1000000 / (freqHz * 2);  // 微秒
  unsigned long endTime = millis() + durationMs;

  while (millis() < endTime) {
    dacWrite(SPEAKER_PIN, 255);
    delayMicroseconds(halfPeriod);
    dacWrite(SPEAKER_PIN, 0);
    delayMicroseconds(halfPeriod);
  }
  dacWrite(SPEAKER_PIN, 0);
}

// 双音提示（开机音效）
void speakerStartupTone() {
  if (!SPEAKER_ENABLED) return;
  speakerBeep(880, 100);
  delay(50);
  speakerBeep(1320, 150);
}

// 错误音效
void speakerErrorTone() {
  if (!SPEAKER_ENABLED) return;
  speakerBeep(300, 200);
  delay(50);
  speakerBeep(200, 300);
}

#endif // SPEAKER_H

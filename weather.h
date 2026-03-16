/*
 * weather.h - OpenWeatherMap 天气数据
 */

#ifndef WEATHER_H
#define WEATHER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============ Weather Data ============
float weatherTemp = 0;
float weatherHumidity = 0;
float weatherFeelsLike = 0;
int weatherCode = 0;
char weatherDesc[64] = "";
bool weatherValid = false;

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  char url[256];
  snprintf(url, sizeof(url),
    "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&appid=%s&units=%s",
    WEATHER_CITY, WEATHER_COUNTRY, WEATHER_API_KEY, WEATHER_UNITS
  );

  Serial.printf("[Weather] Fetching: %s\n", WEATHER_CITY);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    // ArduinoJson v7
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      weatherTemp = doc["main"]["temp"].as<float>();
      weatherHumidity = doc["main"]["humidity"].as<float>();
      weatherFeelsLike = doc["main"]["feels_like"].as<float>();
      weatherCode = doc["weather"][0]["id"].as<int>();

      const char* desc = doc["weather"][0]["description"];
      strncpy(weatherDesc, desc, sizeof(weatherDesc) - 1);
      weatherDesc[sizeof(weatherDesc) - 1] = '\0';

      // 首字母大写
      if (weatherDesc[0] >= 'a' && weatherDesc[0] <= 'z') {
        weatherDesc[0] -= 32;
      }

      weatherValid = true;
      Serial.printf("[Weather] %.1f°C, %s\n", weatherTemp, weatherDesc);
    } else {
      Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("[Weather] HTTP error: %d\n", httpCode);
  }

  http.end();
}

// 根据天气代码返回简易图标字符（后续可替换为bitmap）
const char* getWeatherIcon(int code) {
  if (code >= 200 && code < 300) return "Storm";
  if (code >= 300 && code < 400) return "Drizzle";
  if (code >= 500 && code < 600) return "Rain";
  if (code >= 600 && code < 700) return "Snow";
  if (code >= 700 && code < 800) return "Fog";
  if (code == 800)               return "Clear";
  if (code > 800)                return "Cloudy";
  return "?";
}

#endif // WEATHER_H

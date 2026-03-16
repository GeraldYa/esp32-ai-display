/*
 * news.h - NewsAPI 新闻摘要
 */

#ifndef NEWS_H
#define NEWS_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============ News Data ============
struct NewsItem {
  char title[128];
  char source[32];
  char url[256];
};

NewsItem newsItems[NEWS_MAX_ARTICLES];
int newsCount = 0;
bool newsValid = false;
int newsScrollOffset = 0;

void fetchNews() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  char url[512];
  snprintf(url, sizeof(url),
    "https://newsapi.org/v2/top-headlines?country=%s&category=%s&pageSize=%d&apiKey=%s",
    NEWS_COUNTRY, NEWS_CATEGORY, NEWS_MAX_ARTICLES, NEWS_API_KEY
  );

  Serial.println("[News] Fetching headlines...");
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    // 用 filter 减少内存占用
    JsonDocument filter;
    filter["articles"][0]["title"] = true;
    filter["articles"][0]["source"]["name"] = true;
    filter["articles"][0]["url"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (!err) {
      JsonArray articles = doc["articles"].as<JsonArray>();
      newsCount = 0;

      for (JsonObject article : articles) {
        if (newsCount >= NEWS_MAX_ARTICLES) break;

        const char* title = article["title"];
        const char* source = article["source"]["name"];
        const char* articleUrl = article["url"];

        if (title) {
          strncpy(newsItems[newsCount].title, title, sizeof(newsItems[newsCount].title) - 1);
          newsItems[newsCount].title[sizeof(newsItems[newsCount].title) - 1] = '\0';
        }
        if (source) {
          strncpy(newsItems[newsCount].source, source, sizeof(newsItems[newsCount].source) - 1);
          newsItems[newsCount].source[sizeof(newsItems[newsCount].source) - 1] = '\0';
        }
        if (articleUrl) {
          strncpy(newsItems[newsCount].url, articleUrl, sizeof(newsItems[newsCount].url) - 1);
          newsItems[newsCount].url[sizeof(newsItems[newsCount].url) - 1] = '\0';
        }

        newsCount++;
      }

      newsValid = true;
      newsScrollOffset = 0;
      Serial.printf("[News] Got %d articles\n", newsCount);
    } else {
      Serial.printf("[News] JSON error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("[News] HTTP error: %d\n", httpCode);
  }

  http.end();
}

#endif // NEWS_H

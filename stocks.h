/*
 * stocks.h - Finnhub 美股行情
 */

#ifndef STOCKS_H
#define STOCKS_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============ Stock Data ============
struct StockData {
  char symbol[10];
  float price;
  float prevClose;
  float changePercent;
  bool valid;
};

StockData stocks[STOCK_COUNT];
bool stocksValid = false;

void fetchStocks() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("[Stocks] Fetching quotes...");
  HTTPClient http;
  bool anyValid = false;

  for (int i = 0; i < STOCK_COUNT; i++) {
    char url[256];
    snprintf(url, sizeof(url),
      "https://finnhub.io/api/v1/quote?symbol=%s&token=%s",
      STOCK_SYMBOLS[i], STOCK_API_KEY
    );

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload);

      if (!err) {
        strncpy(stocks[i].symbol, STOCK_SYMBOLS[i], sizeof(stocks[i].symbol));
        stocks[i].price = doc["c"].as<float>();        // Current price
        stocks[i].prevClose = doc["pc"].as<float>();    // Previous close

        if (stocks[i].prevClose > 0) {
          stocks[i].changePercent =
            ((stocks[i].price - stocks[i].prevClose) / stocks[i].prevClose) * 100.0;
        } else {
          stocks[i].changePercent = 0;
        }

        stocks[i].valid = true;
        anyValid = true;

        Serial.printf("[Stocks] %s: $%.2f (%+.2f%%)\n",
          stocks[i].symbol, stocks[i].price, stocks[i].changePercent);
      }
    } else {
      Serial.printf("[Stocks] %s HTTP error: %d\n", STOCK_SYMBOLS[i], httpCode);
      stocks[i].valid = false;
    }

    http.end();
    delay(200);  // Rate limit: 别太快，Finnhub免费版60次/分钟
  }

  stocksValid = anyValid;
}

#endif // STOCKS_H

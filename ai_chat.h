/*
 * ai_chat.h - AI对话模块
 *
 * ESP32内存有限，直接跑HTTPS调Claude/OpenAI会很吃力。
 * 推荐方案：在你的电脑/服务器上跑一个简单的HTTP proxy，
 * ESP32只需要发HTTP请求给proxy，proxy再转发给AI API。
 *
 * Proxy示例 (Node.js / Python) 见 README.md
 */

#ifndef AI_CHAT_H
#define AI_CHAT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============ AI Chat State ============
char aiResponse[512] = "";
bool aiWaiting = false;
bool aiValid = false;

void sendAIQuery(const char* query) {
  if (!AI_ENABLED || WiFi.status() != WL_CONNECTED) return;

  aiWaiting = true;
  aiValid = false;
  Serial.printf("[AI] Sending query: %s\n", query);

  HTTPClient http;
  http.begin(AI_PROXY_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);  // AI可能慢，给15秒

  // 构建请求
  JsonDocument reqDoc;
  reqDoc["query"] = query;
  reqDoc["max_tokens"] = AI_MAX_TOKENS;

  String reqBody;
  serializeJson(reqDoc, reqBody);

  int httpCode = http.POST(reqBody);

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument resDoc;
    DeserializationError err = deserializeJson(resDoc, payload);

    if (!err) {
      const char* response = resDoc["response"];
      if (response) {
        strncpy(aiResponse, response, sizeof(aiResponse) - 1);
        aiResponse[sizeof(aiResponse) - 1] = '\0';
        aiValid = true;
        Serial.printf("[AI] Response: %s\n", aiResponse);
      }
    } else {
      strncpy(aiResponse, "Parse error", sizeof(aiResponse));
      Serial.printf("[AI] JSON error: %s\n", err.c_str());
    }
  } else {
    snprintf(aiResponse, sizeof(aiResponse), "Error: HTTP %d", httpCode);
    Serial.printf("[AI] HTTP error: %d\n", httpCode);
  }

  http.end();
  aiWaiting = false;
}

#endif // AI_CHAT_H

// OTA + Melody Test - wireless code push ready
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <driver/i2s.h>
#include <math.h>

#define WIFI_SSID     "work"
#define WIFI_PASSWORD "REDACTED_WIFI_PASS"

#define I2S_BCLK 1
#define I2S_LRC  2
#define I2S_DIN  3
#define SAMPLE_RATE 44100

void i2sInit() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

void playTone(float freq, int durationMs, float volume = 0.2) {
  int totalSamples = SAMPLE_RATE * durationMs / 1000;
  int fadeLen = SAMPLE_RATE * 10 / 1000;
  int16_t buf[128];
  int bufIdx = 0;
  for (int i = 0; i < totalSamples; i++) {
    float t = (float)i / SAMPLE_RATE;
    float sample = sin(2.0 * M_PI * freq * t);
    float envelope = 1.0;
    if (i < fadeLen) envelope = (float)i / fadeLen;
    if (i > totalSamples - fadeLen) envelope = (float)(totalSamples - i) / fadeLen;
    int16_t val = (int16_t)(sample * envelope * volume * 32000);
    buf[bufIdx++] = val;
    buf[bufIdx++] = val;
    if (bufIdx >= 128) {
      size_t written;
      i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
      bufIdx = 0;
    }
  }
  if (bufIdx > 0) {
    size_t written;
    i2s_write(I2S_NUM_0, buf, bufIdx * sizeof(int16_t), &written, portMAX_DELAY);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[OTA Display] Starting...");

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    // OTA Setup
    ArduinoOTA.setHostname("esp32-ai-display");
    ArduinoOTA.onStart([]() { Serial.println("[OTA] Start"); });
    ArduinoOTA.onEnd([]() { Serial.println("\n[OTA] Done! Rebooting..."); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("[OTA] %u%%\r", progress * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("[OTA] Error %u\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready! You can unplug USB now.");

    // Play a happy chime to confirm ready
    i2sInit();
    playTone(523, 150, 0.15);  // C5
    playTone(659, 150, 0.15);  // E5
    playTone(784, 300, 0.15);  // G5
    i2s_zero_dma_buffer(I2S_NUM_0);
  } else {
    Serial.println("\n[WiFi] Failed!");
  }
}

void loop() {
  ArduinoOTA.handle();  // Listen for OTA updates
  delay(10);
}

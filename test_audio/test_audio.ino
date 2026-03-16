// Audio quality test - download WAV from server and play via I2S
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

#define WIFI_SSID     "work"
#define WIFI_PASSWORD "REDACTED_WIFI_PASS"
#define AUDIO_URL     "http://192.168.2.106:3100/api/audio/test_stereo.wav"

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
    .dma_buf_count = 16,
    .dma_buf_len = 512,
    .use_apll = true,
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

bool downloadAndPlay(const char* url) {
  Serial.printf("[Audio] Downloading %s\n", url);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();

  if (code != 200) {
    Serial.printf("[Audio] HTTP error: %d\n", code);
    http.end();
    return false;
  }

  int totalLen = http.getSize();
  Serial.printf("[Audio] Size: %d bytes\n", totalLen);

  // Allocate in PSRAM
  uint8_t *wavBuf = (uint8_t *)ps_malloc(totalLen);
  if (!wavBuf) {
    Serial.println("[Audio] PSRAM alloc failed");
    http.end();
    return false;
  }

  // Download entire file
  WiFiClient *stream = http.getStreamPtr();
  int bytesRead = 0;
  while (bytesRead < totalLen) {
    int avail = stream->available();
    if (avail) {
      int toRead = min(avail, totalLen - bytesRead);
      stream->readBytes(wavBuf + bytesRead, toRead);
      bytesRead += toRead;
    }
    delay(1);
  }
  http.end();
  Serial.printf("[Audio] Downloaded %d bytes\n", bytesRead);

  // Skip WAV header (44 bytes)
  int dataOffset = 44;
  int dataLen = totalLen - dataOffset;
  uint8_t *pcmData = wavBuf + dataOffset;

  // Play PCM data via I2S
  Serial.println("[Audio] Playing...");
  int chunkSize = 2048;
  int played = 0;
  while (played < dataLen) {
    int toWrite = min(chunkSize, dataLen - played);
    size_t written = 0;
    i2s_write(I2S_NUM_0, pcmData + played, toWrite, &written, portMAX_DELAY);
    played += written;
  }

  // Silence at end to flush DMA
  uint8_t silence[512] = {0};
  for (int i = 0; i < 4; i++) {
    size_t w;
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &w, portMAX_DELAY);
  }

  free(wavBuf);
  Serial.println("[Audio] Done!");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[Audio Test] Starting...");

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WiFi] Failed!");
    return;
  }
  Serial.printf("\n[WiFi] OK: %s\n", WiFi.localIP().toString().c_str());

  // OTA
  ArduinoOTA.setHostname("esp32-ai-display");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");

  // I2S
  i2sInit();
  Serial.println("[I2S] Initialized");

  // Download and play
  downloadAndPlay(AUDIO_URL);
}

void loop() {
  ArduinoOTA.handle();
  delay(10);
}

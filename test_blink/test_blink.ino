// 最简单的测试：ESP32-S3 内置 RGB LED 闪烁
// 如果板子上的灯在闪，说明烧录成功了

// ESP32-S3 DevKit 内置 RGB LED 通常在 GPIO48 或 GPIO38
#define LED_PIN 2  // 尝试通用GPIO2，大部分板子有

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("ESP32-S3 Blink Test - OK!");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("ON");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  Serial.println("OFF");
  delay(500);
}

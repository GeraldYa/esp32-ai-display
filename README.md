# ESP32 掌上 AI 显示器

## 硬件清单
| 组件 | 型号 | 备注 |
|------|------|------|
| 主控 | ESP32 (WROOM-32) | 需要带SPI和I2C的版本 |
| 屏幕 | 3.5" ST7796 CTP | 320x480, SPI接口, 电容触控(I2C) |
| 扬声器 | 小喇叭 | 接DAC引脚(GPIO25)，或用I2S+功放 |

## 接线图

```
ESP32          ST7796 屏幕 (SPI)
─────          ─────────────────
GPIO23 (MOSI)  → SDA/MOSI
GPIO18 (SCLK)  → SCK
GPIO15         → CS
GPIO2          → DC/RS
GPIO4          → RST
GPIO21         → BL (背光，可选PWM调光)
3.3V           → VCC
GND            → GND

ESP32          CTP触控 (I2C)
─────          ──────────────
GPIO21 (SDA)   → SDA
GPIO22 (SCL)   → SCL
GPIO36         → INT (可选)
3.3V           → VCC
GND            → GND

ESP32          扬声器
─────          ──────
GPIO25 (DAC1)  → Speaker +
GND            → Speaker -
```

> ⚠️ 注意：如果你的屏幕模块是一体板（SPI+触控都在同一个排针上），
> 引脚分配可能不同，请参考你购买的模块文档。I2C的SDA和SPI的MOSI
> 都用到GPIO23/21的话需要确认是否冲突。

## 开发环境搭建

### 1. 安装 Arduino IDE
- 下载: https://www.arduino.cc/en/software
- 推荐 Arduino IDE 2.x

### 2. 添加 ESP32 开发板
- Arduino IDE → File → Preferences
- Additional Boards Manager URLs 添加:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- Tools → Board → Boards Manager → 搜索 "esp32" → 安装

### 3. 安装依赖库
在 Arduino IDE 的 Library Manager 中安装:

| 库名 | 用途 |
|------|------|
| TFT_eSPI | ST7796屏幕驱动 + 触控 |
| ArduinoJson (v7) | 解析API返回的JSON |

### 4. 配置 TFT_eSPI

**这步很关键！** TFT_eSPI 需要手动改配置文件。

找到库文件夹（通常在 `~/Arduino/libraries/TFT_eSPI/`），编辑 `User_Setup.h`：

```cpp
// 注释掉默认驱动，启用 ST7796
// #define ILI9341_DRIVER
#define ST7796_DRIVER

#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// SPI pins
#define TFT_MISO   19
#define TFT_MOSI   23
#define TFT_SCLK   18
#define TFT_CS     15
#define TFT_DC     2
#define TFT_RST    4
#define TFT_BL     21

// SPI频率 (ST7796支持较高频率)
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// CTP触控用I2C，不需要SPI touch CS
// #define TOUCH_CS  -1
```

### 5. 配置 API Keys

编辑 `config.h`，填入你的：
- WiFi名称和密码
- OpenWeatherMap API Key (免费注册)
- Finnhub API Key (免费注册)
- NewsAPI Key (免费注册)

### 6. 编译上传
- Tools → Board → ESP32 Dev Module
- Tools → Port → 选择你的ESP32串口
- Upload Speed: 921600
- 点 Upload

## 项目结构

```
esp32-ai-display/
├── esp32-ai-display.ino   # 主程序（setup/loop/触控/WiFi）
├── config.h               # 所有配置项（WiFi/API/引脚/颜色）
├── ui.h                   # UI绘制（各屏幕布局+导航栏）
├── weather.h              # 天气数据获取
├── stocks.h               # 股票行情获取
├── news.h                 # 新闻获取
├── ai_chat.h              # AI对话（需proxy）
├── speaker.h              # 扬声器音效
└── README.md              # 本文件
```

## 屏幕说明

横屏模式 (480x320)，底部40px为导航栏，4个tab：

| Tab | 功能 | 刷新频率 |
|-----|------|----------|
| Clock | 时钟+天气 | 时钟1秒，天气10分钟 |
| Stock | 6只美股行情 | 1分钟 |
| News | 科技新闻标题 | 30分钟 |
| AI | AI对话 | 手动触发 |

## AI对话 Proxy 方案

ESP32直接调HTTPS的Claude/OpenAI API会很吃力（内存、TLS握手）。
推荐在局域网内跑一个轻量proxy：

### Python proxy 示例 (Flask)
```python
from flask import Flask, request, jsonify
import anthropic

app = Flask(__name__)
client = anthropic.Anthropic(api_key="your-key")

@app.route('/chat', methods=['POST'])
def chat():
    data = request.json
    msg = client.messages.create(
        model="claude-haiku-4-5-20251001",  # 便宜够用
        max_tokens=data.get('max_tokens', 150),
        messages=[{"role": "user", "content": data['query']}]
    )
    return jsonify({"response": msg.content[0].text})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

然后把 `config.h` 里的 `AI_PROXY_URL` 改成你电脑的局域网IP。

## 后续扩展思路
- 🎨 自定义表盘主题（深色/浅色/赛博朋克）
- 📊 加密货币行情 (CoinGecko API)
- 🔔 定时提醒/番茄钟
- 🎵 I2S音频 + TTS语音播报
- 🌐 OTA无线更新固件
- 📱 蓝牙配网（不用硬编码WiFi密码）
- 💬 语音输入（加麦克风 + Whisper API）

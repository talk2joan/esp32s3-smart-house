/*
 * ============================================================
 *  温湿度模块：AI 语音对话查询 DHT11
 * ============================================================
 *  功能：
 *   1. 每 2 秒采集一次温湿度，上报 temperature / humidity 字段
 *   2. 小程序近实时显示温湿度
 *   3. AI 语音可查询当前温度、湿度（“现在温度是多少”）
 *   4. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：KY-015 / DHT11 数据脚 → GPIO16（signal→16, vcc→3.3V, gnd→GND）
 *  小程序：导入 miniprogram-layouts/温湿度模块-布局.txt
 * ============================================================
 */
#include <house_config.h>
#include <DHT.h>

#define DHT_PIN   16      // DHT11 数据引脚
#define DHT_TYPE  DHT11
#define READ_PERIOD_MS  2000   // 采集周期 2 秒

DHT dht(DHT_PIN, DHT_TYPE);

float temp = 0;                    // 当前温度
float humi = 0;                    // 当前湿度
uint32_t last_dht_time = 0;        // 上次采集时间

void setup() {
  dht.begin();
  wkHouseInit();

  Serial.println("[温湿度模块] AI语音对话查询DHT11 已启动");
}

void loop() {
  wkHouseLoop();

  /* ---------- 每 2 秒采集并上报温湿度 ---------- */
  if (millis() - last_dht_time >= READ_PERIOD_MS) {
    last_dht_time = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {   // DHT11 偶发读取失败，失败时保留旧值
      temp = t;
      humi = h;
      Serial.printf("温度: %.1f ℃，湿度: %.1f %%\n", temp, humi);
      wkPublishField("temperature", String(temp));
      wkPublishField("humidity", String(humi));
    }
  }

  /* ---------- 音量消息 ---------- */
  if (wkTakeMessage()) {
    wkHouseHandleVolume();
    wkFinishMessage();
  }

  /* ---------- 向 AI 提供状态：音量 + 温度 + 湿度 ---------- */
  wkSensorClear();
  wkSensorAddNumber("volume", wkGetVolume());
  wkSensorAddNumber("temperature", temp);
  wkSensorAddNumber("humidity", humi);
  wkVoiceSetSensors();
}

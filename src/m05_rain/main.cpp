/*
 * ============================================================
 *  雨水模块：AI 语音对话查询雨水传感器
 * ============================================================
 *  功能：
 *   1. 每 1 秒读取雨水传感器模拟量（0~4095）
 *   2. 阈值判断：数值 > 2000 → 无雨；≤ 2000 → 有雨
 *   3. 上报 rain（原始值）和 rain_status（有雨/无雨）
 *   4. AI 语音可查询雨水状态（“有没有下雨”）
 *   5. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：雨水/水位传感器 OUT → GPIO6（VCC→3.3V, GND→GND）
 *  说明：阈值为参考值，不同模块请按实测调整 RAIN_THRESHOLD。
 *  小程序：导入 miniprogram-layouts/雨水模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define RAIN_PIN        6      // 雨水传感器模拟输出
#define RAIN_THRESHOLD  2000   // 判断阈值：>2000 无雨，≤2000 有雨
#define READ_PERIOD_MS  1000   // 采集周期 1 秒

int    rain_value = 0;         // 当前模拟值
String rain_status = "";       // “有雨”/“无雨”
uint32_t last_rain_time = 0;

void setup() {
  wkHouseInit();

  Serial.println("[雨水模块] AI语音对话查询雨水传感器 已启动");
}

void loop() {
  wkHouseLoop();

  /* ---------- 每 1 秒采集并上报雨水数据 ---------- */
  if (millis() - last_rain_time >= READ_PERIOD_MS) {
    last_rain_time = millis();

    rain_value = analogRead(RAIN_PIN);
    rain_status = (rain_value > RAIN_THRESHOLD) ? "无雨" : "有雨";

    wkPublishField("rain", String(rain_value));
    wkPublishField("rain_status", rain_status);
  }

  /* ---------- 音量消息 ---------- */
  if (wkTakeMessage()) {
    wkHouseHandleVolume();
    wkFinishMessage();
  }

  /* ---------- 向 AI 提供状态 ---------- */
  wkSensorClear();
  wkSensorAddNumber("volume", wkGetVolume());
  wkSensorAddNumber("rain", rain_value);
  wkSensorAddString("rain_status", rain_status);
  wkVoiceSetSensors();
}

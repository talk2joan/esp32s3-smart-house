/*
 * ============================================================
 *  人体感应模块：AI 语音对话查询人体红外（PIR）
 * ============================================================
 *  功能：
 *   1. 每 1 秒读取人体红外（PIR）数字输出
 *   2. 上报 pir_value（1=有人/0=无人）和 pir_status（状态文字）
 *   3. AI 语音可查询人体状态（“现在有人吗”）
 *   4. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：HC-SR501 人体红外 OUT → GPIO14（VCC→3.3V, GND→GND）
 *  说明：高电平=检测到有人；PIR 上电后需数十秒稳定时间，属正常现象。
 *  小程序：导入 miniprogram-layouts/人体感应模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define PIR_PIN         14     // PIR 数字输出
#define READ_PERIOD_MS  1000   // 检测周期 1 秒

int    pir_value = 0;          // 1=有人，0=无人
String pir_status = "";        // “检测到有人”/“检测到无人”
uint32_t last_pir_time = 0;

void setup() {
  pinMode(PIR_PIN, INPUT);
  wkHouseInit();

  Serial.println("[人体感应模块] AI语音对话查询人体红外传感器 已启动");
}

void loop() {
  wkHouseLoop();

  /* ---------- 每 1 秒检测并上报人体状态 ---------- */
  if (millis() - last_pir_time >= READ_PERIOD_MS) {
    last_pir_time = millis();

    pir_value = digitalRead(PIR_PIN);
    pir_status = (pir_value == 1) ? "检测到有人" : "检测到无人";

    wkPublishField("pir_value", String(pir_value));
    wkPublishField("pir_status", pir_status);
  }

  /* ---------- 音量消息 ---------- */
  if (wkTakeMessage()) {
    wkHouseHandleVolume();
    wkFinishMessage();
  }

  /* ---------- 向 AI 提供状态 ---------- */
  wkSensorClear();
  wkSensorAddNumber("volume", wkGetVolume());
  wkSensorAddNumber("pir_value", pir_value);
  wkSensorAddString("pir_status", pir_status);
  wkVoiceSetSensors();
}

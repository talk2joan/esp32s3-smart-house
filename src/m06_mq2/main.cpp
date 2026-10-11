/*
 * ============================================================
 *  烟雾模块：AI 语音对话查询 MQ2 可燃气体/烟雾传感器
 * ============================================================
 *  功能：
 *   1. 每 1 秒读取 MQ2 模拟量（0~4095）
 *   2. 阈值判断：数值 > 2200 → 烟雾浓度较高；≤ 2200 → 烟雾较少
 *   3. 上报 mq2（原始值）和 mq2_status（状态文字）
 *   4. AI 语音可查询烟雾状态（“现在烟雾浓度怎么样”）
 *   5. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：MQ-2 模块 A0 → GPIO5（VCC→5V, GND→GND）
 *  说明：ADC 原始值仅用于状态区分，不代表 ppm 浓度；
 *        MQ2 上电需要预热，阈值请按现场稳定读数调整 MQ2_THRESHOLD。
 *  小程序：导入 miniprogram-layouts/烟雾模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define MQ2_PIN        5      // MQ2 模拟输出
#define MQ2_THRESHOLD  2200   // 判断阈值：>2200 烟雾浓度较高
#define READ_PERIOD_MS 1000   // 采集周期 1 秒

int    mq2_value = 0;          // 当前 ADC 值
String mq2_status = "";        // “烟雾浓度较高”/“烟雾较少”
uint32_t last_mq2_time = 0;

void setup() {
  wkHouseInit();

  Serial.println("[烟雾模块] AI语音对话查询MQ2传感器 已启动");
}

void loop() {
  wkHouseLoop();

  /* ---------- 每 1 秒采集并上报 MQ2 数据 ---------- */
  if (millis() - last_mq2_time >= READ_PERIOD_MS) {
    last_mq2_time = millis();

    mq2_value = analogRead(MQ2_PIN);
    mq2_status = (mq2_value > MQ2_THRESHOLD) ? "烟雾浓度较高" : "烟雾较少";

    wkPublishField("mq2", String(mq2_value));
    wkPublishField("mq2_status", mq2_status);
  }

  /* ---------- 音量消息 ---------- */
  if (wkTakeMessage()) {
    wkHouseHandleVolume();
    wkFinishMessage();
  }

  /* ---------- 向 AI 提供状态 ---------- */
  wkSensorClear();
  wkSensorAddNumber("volume", wkGetVolume());
  wkSensorAddNumber("mq2", mq2_value);
  wkSensorAddString("mq2_status", mq2_status);
  wkVoiceSetSensors();
}

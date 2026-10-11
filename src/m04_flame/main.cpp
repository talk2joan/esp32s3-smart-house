/*
 * ============================================================
 *  火焰模块：AI 语音对话查询火焰传感器
 * ============================================================
 *  功能：
 *   1. 每 500ms 读取火焰传感器数字输出
 *   2. 上报 flame_value（1=有火/0=无火）和 flame_status（有火/无火）
 *   3. AI 语音可查询火焰状态（“有没有检测到火焰”）
 *   4. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：KY-026 火焰传感器 D0 → GPIO7（+→3.3V, G→GND）
 *  注意：本程序按【高电平=有火】判断。
 *        若你的模块逻辑相反，把 FLAME_ACTIVE_LEVEL 改为 LOW 即可。
 *  小程序：导入 miniprogram-layouts/火焰模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define FLAME_PIN           7      // 火焰传感器数字输出
#define FLAME_ACTIVE_LEVEL  HIGH   // 有火时的电平
#define READ_PERIOD_MS      500    // 检测周期 500ms

int    flame_value = 0;            // 1=有火，0=无火
String flame_status = "";          // “有火”/“无火”
uint32_t last_flame_time = 0;

void setup() {
  pinMode(FLAME_PIN, INPUT);
  wkHouseInit();

  Serial.println("[火焰模块] AI语音对话查询火焰传感器 已启动");
}

void loop() {
  wkHouseLoop();

  /* ---------- 每 500ms 检测并上报火焰状态 ---------- */
  if (millis() - last_flame_time >= READ_PERIOD_MS) {
    last_flame_time = millis();

    flame_value = (digitalRead(FLAME_PIN) == FLAME_ACTIVE_LEVEL) ? 1 : 0;
    flame_status = flame_value ? "有火" : "无火";

    wkPublishField("flame_value", String(flame_value));
    wkPublishField("flame_status", flame_status);
  }

  /* ---------- 音量消息 ---------- */
  if (wkTakeMessage()) {
    wkHouseHandleVolume();
    wkFinishMessage();
  }

  /* ---------- 向 AI 提供状态 ---------- */
  wkSensorClear();
  wkSensorAddNumber("volume", wkGetVolume());
  wkSensorAddNumber("flame_value", flame_value);
  wkSensorAddString("flame_status", flame_status);
  wkVoiceSetSensors();
}

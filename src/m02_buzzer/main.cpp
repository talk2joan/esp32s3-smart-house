/*
 * ============================================================
 *  蜂鸣器模块：AI 语音对话控制蜂鸣器
 * ============================================================
 *  功能：
 *   1. 小程序 / AI 语音远程开关蜂鸣器（btn: power-on / power-off）
 *   2. 控制后回传“蜂鸣器已打开 / 蜂鸣器已关闭”
 *   3. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：KY-012 有源蜂鸣器 → GPIO18（高电平响，低电平停）
 *  说明：数字电平控制，适合有源蜂鸣器；无源蜂鸣器需改用 PWM。
 *  小程序：导入 miniprogram-layouts/蜂鸣器模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define BUZZER_PIN  18   // 蜂鸣器，高电平打开

bool buzzer_on = false;  // 蜂鸣器当前状态

void setup() {
  /* 蜂鸣器初始化：上电默认关闭 */
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  buzzer_on = false;

  wkHouseInit();

  Serial.println("[蜂鸣器模块] AI语音对话控制蜂鸣器 已启动");
}

void loop() {
  wkHouseLoop();

  if (wkTakeMessage()) {
    if (wkMsgMatch("btn", "power-on")) {          // 打开蜂鸣器
      digitalWrite(BUZZER_PIN, HIGH);
      buzzer_on = true;
      wkPublishMatchedField("蜂鸣器已打开");
    } else if (wkMsgMatch("btn", "power-off")) {  // 关闭蜂鸣器
      digitalWrite(BUZZER_PIN, LOW);
      buzzer_on = false;
      wkPublishMatchedField("蜂鸣器已关闭");
    } else {
      wkHouseHandleVolume();
    }
    wkFinishMessage();
  }

  /* 向 AI 提供状态：蜂鸣器开关 + 当前音量 */
  wkSensorClear();
  wkSensorAddBool("buzzer_on", buzzer_on);
  wkSensorAddNumber("volume", wkGetVolume());
  wkVoiceSetSensors();
}

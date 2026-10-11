/*
 * ============================================================
 *  风扇模块：AI 语音对话控制风扇
 * ============================================================
 *  功能：
 *   1. 小程序 / AI 语音远程开关风扇（btn: power-on / power-off）
 *   2. PWM 调速驱动：开启输出占空比 160，关闭输出 0
 *   3. 控制后回传“风扇开了 / 风扇关了”
 *   4. AI 语音或小程序滑块调节音量（ran: 0~100）
 *
 *  硬件：直流电机/风扇模块
 *        INA → GPIO10（PWM，20kHz / 8bit / 通道0）
 *        INB → GPIO11（辅助脚，始终输出低电平）
 *  小程序：导入 miniprogram-layouts/风扇模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define FAN_INA_PIN   10    // PWM 输出
#define FAN_INB_PIN   11    // 辅助控制脚，保持 LOW
#define PWM_CHANNEL   0
#define PWM_FREQ      20000 // 20 kHz
#define PWM_RES_BITS  8     // 8 位分辨率
#define FAN_ON_DUTY   160   // 开启占空比（0~255）

bool fan_on = false;        // 风扇当前状态

void setup() {
  /* 风扇初始化：上电默认关闭 */
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES_BITS);
  ledcAttachPin(FAN_INA_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);
  pinMode(FAN_INB_PIN, OUTPUT);
  digitalWrite(FAN_INB_PIN, LOW);
  fan_on = false;

  wkHouseInit();

  Serial.println("[风扇模块] AI语音对话控制风扇 已启动");
}

void loop() {
  wkHouseLoop();

  if (wkTakeMessage()) {
    if (wkMsgMatch("btn", "power-on")) {          // 打开风扇
      ledcWrite(PWM_CHANNEL, FAN_ON_DUTY);
      digitalWrite(FAN_INB_PIN, LOW);
      fan_on = true;
      wkPublishMatchedField("风扇开了");
    } else if (wkMsgMatch("btn", "power-off")) {  // 关闭风扇
      ledcWrite(PWM_CHANNEL, 0);
      digitalWrite(FAN_INB_PIN, LOW);
      fan_on = false;
      wkPublishMatchedField("风扇关了");
    } else {
      wkHouseHandleVolume();
    }
    wkFinishMessage();
  }

  /* 向 AI 提供状态：风扇开关 + 当前音量 */
  wkSensorClear();
  wkSensorAddBool("fan_on", fan_on);
  wkSensorAddNumber("volume", wkGetVolume());
  wkVoiceSetSensors();
}

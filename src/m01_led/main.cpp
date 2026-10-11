/*
 * ============================================================
 *  灯光模块：AI 语音对话控制 LED
 * ============================================================
 *  功能：
 *   1. 小程序 / AI 语音远程开关 LED（btn: power-on / power-off）
 *   2. 控制后回传“灯开了 / 灯关了”
 *   3. AI 语音或小程序滑块调节音量（ran: 0~100）
 *   4. 唤醒词「你好小智」发起 AI 对话，AI 可查询 LED 与音量状态
 *
 *  硬件：LED → GPIO17（高电平点亮）
 *  小程序：导入 miniprogram-layouts/灯光模块-布局.txt
 * ============================================================
 */
#include <house_config.h>

#define LED_PIN  17   // LED，高电平点亮

bool led_on = false;  // LED 当前状态（同步给 AI 查询）

void setup() {
  /* LED 初始化：上电默认关闭 */
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  led_on = false;

  wkHouseInit();     // WiFi + MQTT + AI 语音（见 include/house_config.h）

  Serial.println("[灯光模块] AI语音对话控制LED 已启动");
}

void loop() {
  wkHouseLoop();     // 云端 + 语音链路维护

  /* ---------- 处理云端下行消息 ---------- */
  if (wkTakeMessage()) {
    if (wkMsgMatch("btn", "power-on")) {          // 开灯
      digitalWrite(LED_PIN, HIGH);
      led_on = true;
      wkPublishMatchedField("灯开了");
    } else if (wkMsgMatch("btn", "power-off")) {  // 关灯
      digitalWrite(LED_PIN, LOW);
      led_on = false;
      wkPublishMatchedField("灯关了");
    } else {
      wkHouseHandleVolume();                     // ran 数值 → 音量
    }
    wkFinishMessage();
  }

  /* ---------- 向 AI 语音系统提供设备状态 ---------- */
  wkSensorClear();
  wkSensorAddBool("led_on", led_on);          // LED 开关状态
  wkSensorAddNumber("volume", wkGetVolume()); // 当前音量
  wkVoiceSetSensors();
}

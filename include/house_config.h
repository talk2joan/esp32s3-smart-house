/*
 * ============================================================
 *  赛博自建房 · 全屋公共配置
 *  —— WiFi、维控云端参数、语音硬件引脚、公共初始化流程
 *  各功能模块共用这一份配置，改 WiFi 只改这里。
 * ============================================================
 *  【按需修改】
 *   1. WIFI_SSID / WIFI_PASSWORD：你的 2.4GHz WiFi
 *   2. 如果你的麦克风 / 功放 / 按键引脚不同，改下面的宏
 * ============================================================
 */
#pragma once
#include <Arduino.h>
#include <WCMGC_Comm.h>

/* ---------- ① WiFi（仅支持 2.4GHz）---------- */
#define WIFI_SSID      "wcmgc"
#define WIFI_PASSWORD  "12344321"

/* ---------- ② 语音硬件引脚（本套件实际接线）----------
 *  INMP441 麦克风：WS=35, SCK=36, SD=37
 *  MAX98357 功放 ：LRC=48, BCLK=47, DIN=21
 *  按键模块     ：Signal=15，按下输出高电平
 *  （若你的按键按下为低电平，把 BUTTON_DOWN_LEVEL 改为 LOW）
 */
#define PIN_BUTTON          15
#define BUTTON_DOWN_LEVEL   HIGH
#define PIN_MIC_WS          35
#define PIN_MIC_BCLK        36
#define PIN_MIC_SD          37
#define PIN_SPK_LRCK        48
#define PIN_SPK_BCLK        47
#define PIN_SPK_DIN         21

/* ---------- ③ 维控云端（使用官方服务时无需修改）---------- */
#define WK_MQTT_HOST   "api.wcmgc.com"
#define WK_MQTT_PORT   8883
#define WK_MQTT_USER   "dm_device"                        // 维控教学服务共享认证
#define WK_MQTT_PASS   "TVG9vN8gXAdqd-bbcmihn4YWwR4LMsqr"
#define WK_VOICE_URL   "wss://api.wcmgc.com/devicemaster-api/ws/device-voice"

/*
 * 公共初始化：串口 → WiFi → MQTT(TLS) → 语音硬件 → 唤醒词 → 启动链路
 * 每个模块的 setup() 开头调用一次。初始音量 75。
 */
inline void wkHouseInit() {
  Serial.begin(115200);
  wkEnsureSerial();

  wkSetWifi(WIFI_SSID, WIFI_PASSWORD);

  wkSetMqtt(WK_MQTT_HOST, WK_MQTT_PORT);
  wkSetMqttTls(true);
  wkSetMqttAuth(WK_MQTT_USER, WK_MQTT_PASS);
  wkSetTopicPrefix("device");   // 下行 device/{MAC}/command，上行 device/{MAC}/state

  wkSetWakeWord(true);                                  // 唤醒词「你好小智」
  wkSetButton(PIN_BUTTON, BUTTON_DOWN_LEVEL);
  wkSetMic(PIN_MIC_WS, PIN_MIC_BCLK, PIN_MIC_SD);
  wkSetSpeaker(PIN_SPK_LRCK, PIN_SPK_BCLK, PIN_SPK_DIN);
  wkSetVolume(75);

  wkBeginMqtt();          // WiFi + MQTT 控制通道
  wkSetVoiceWs(WK_VOICE_URL);
  wkBeginVoice();         // I2S + 语音状态机（首次启动解压唤醒模型到 model 分区）
}

/* 每个模块的 loop() 开头必须调用：维持云端与语音链路 */
inline void wkHouseLoop() {
  wkLoopMqtt();
  wkVoiceLoop();
  wkButtonLoop();
}

/*
 * 音量消息处理（各模块共用）：
 * 收到 ran 前缀的数值消息（AI 下发或小程序滑块）→ 设置音量并回传实际音量。
 * 在 wkTakeMessage() 分支里调用，返回 true 表示已处理。
 */
inline bool wkHouseHandleVolume() {
  if (wkMsgMatch("ran", "")) {
    wkSetVolume(wkMsgMatchedInt());
    wkPublishMatchedNumber(wkGetVolume());
    return true;
  }
  return false;
}

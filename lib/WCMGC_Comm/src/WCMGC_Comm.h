#pragma once

#include <Arduino.h>
#include <functional>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
/* 闭源发行包把 BLE 目标打进 libWCMGC_Comm.a 且 whole-archive 链接。
 * Mixly 生成代码只包含本头文件，这里必须拉入 NimBLE，否则客户工程会链接失败。 */
#include <NimBLEDevice.h>

/* 维控通信库 — WiFi/MQTT/语音/BLE，键值透传协议 */

/* ---------- 分项配置 ---------- */
void wkSetWifi(const char* ssid, const char* password);
void wkSetMqtt(const char* host, int port);
void wkSetMqttTls(bool enabled);
void wkSetMqttCaCert(const char* pem); /* 可选覆盖；空则 TLS 使用内置 ISRG Root X1 */
void wkSetMqttAuth(const char* user, const char* password);
void wkSetTopicPrefix(const char* prefix);
void wkSetMqttTopics(const char* subTopic, const char* pubTopic); /* 空=按前缀自动 */
void wkSetMqttHeartbeat(int seconds);
void wkSetVoiceWs(const char* url);
void wkSetDevice(const char* deviceId, const char* billingId);
/**
 * 物理设备自动领证。默认从 voice WS URL 推导同源
 * /api/device-auth/provision；私有部署可显式覆盖完整 HTTPS URL。
 * ESP32 本地生成 secret，只向服务端发送 SHA256 verifier。
 */
void wkSetDeviceProvisionUrl(const char* url);
void wkResetDeviceIdentity(); /* 恢复出厂：清 physical credential 与动态 deviceId */
bool wkDeviceCredentialConfigured();
bool wkDeviceAuthSelfTest();
bool wkVoiceProtocolSelfTest();
void wkSetVoiceAuthPath(const char* authPath);
void wkSetStreamAuthPath(const char* authPath);
/** TLS 默认使用内置 ISRG Root X1；传入 PEM 可覆盖。仅实验室可显式允许 insecure。 */
void wkSetVoiceCaCert(const char* pem);
void wkSetVoiceTlsInsecure(bool enabled);
/** 2=DMV2/PCM 20ms（推荐），1=旧 raw PCM 协议。 */
void wkSetVoiceProtocol(int version);
/* 供库内语音/相机链路共用；每次调用都会生成新 timestamp/nonce。 */
bool wkBuildDeviceWebSocketHeaders(const char* canonicalPath, String& headers);
const char* wkGetVoiceCaCert();
bool wkVoiceTlsInsecureEnabled();
const char* wkGetStreamAuthPath();
void wkSetButton(int pin, int downLevel);
void wkSetMic(int wsPin, int bclkPin, int dinPin);
void wkSetSpeaker(int lrckPin, int bclkPin, int doutPin);
void wkSetDebug(bool enabled);
void wkSetSerialBaud(int baud);
/** 幂等打开串口（默认 115200）。相机/舵机初始化前就会打印，避免卡死时完全没日志。 */
void wkEnsureSerial();

void wkConfig(const char* wifiSsid,
              const char* wifiPassword,
              const char* voiceWsUrl,
              const char* mqttHost,
              int mqttPort,
              const char* deviceId,
              const char* billingId,
              int buttonPin,
              int buttonDownLevel,
              int micWsPin,
              int micBclkPin,
              int micDinPin,
              int spkLrckPin,
              int spkBclkPin,
              int spkDoutPin,
              int serialBaud,
              bool debugLog);

/* 三条独立链路（可按需组合） */
bool wkBeginMqtt();   /* WiFi + MQTT 控制通道 */
void wkLoopMqtt();
bool wkBeginVoice();  /* I2S 音频 + 语音状态机（语音 WS 链） */
void wkVoiceLoop();
bool wkBegin();       /* 便捷：MQTT + 语音一起启动 */
void wkLoop();        /* 便捷：MQTT + 语音一起循环 */

/** 语音唤醒模式（对齐 SmartRecognition-A2）：说「你好小智」开一轮对话；
 *  静音约 3 秒自动结束录音。true 时禁用 BOOT 按键 PTT。须在 wkBeginVoice 前调用。 */
void wkSetWakeWord(bool enable);
bool wkWakeWordEnabled();

bool wkMqttConnected();
void wkMqttSubscribe(const String& topic);
void wkMqttHeartbeat();
void wkPublishOffline();

/* ---------- 按键 / 语音 ---------- */
void wkButtonLoop();
void wkButtonLoop(std::function<void()> beforeStart);
bool wkButtonChanged();
bool wkButtonIsDown();
bool wkButtonIsUp();

bool wkVoiceStart();
bool wkVoiceStop();
void wkVoiceAbort();
/** 播放中立即静音、清旧音频并开启新一轮聆听；无硬件 AEC 时推荐用按键触发。 */
bool wkVoiceBargeIn();
bool wkVoiceReady();
bool wkVoiceOnline(); /* 语音 WS 在线；推流在语音掉线时停发 JPEG */
bool wkVoiceServiceReady(); /* WS hello 已完成；对话中的 REC/WAIT/PLAY 也保持 true */
bool wkVoiceListening();
bool wkVoiceSpeaking();
bool wkVoiceBusy();
const char* wkVoiceState();

struct WkVoiceStats {
    uint32_t reconnects;
    uint32_t txFrames;
    uint32_t txAudioBytes;
    uint32_t rxFrames;
    uint32_t rxAudioBytes;
    uint32_t recordDroppedBytes;
    uint32_t playbackDroppedBytes;
    uint32_t protocolErrors;
    uint32_t authFailures;
    uint32_t lastRttMs;
    uint32_t lastBargeInMs;
};
bool wkVoiceGetStats(WkVoiceStats* out);
void wkVoiceResetStats();
String wkVoiceStatsJson();
/* App 实时预览独占网络/TLS 资源；关闭预览后自动恢复云端语音。 */
void wkVoiceSuspendForPreview();
void wkVoiceResumeAfterPreview();

/* ---------- MQTT 下行 / 上报 ---------- */
bool wkTakeMessage();
void wkFinishMessage();
String wkMsgRaw();

bool wkMsgMatch(const String& prefix, const String& value);
bool wkMsgPrefixHasValue(const String& prefix, const String& command);
bool wkMsgPrefixHasNumber(const String& prefix, float* outNumber);

String wkMsgMatchedKey();
String wkMsgMatchedValue();
int wkMsgMatchedInt();
float wkMsgMatchedFloat();
bool wkMsgMatchedIsNumber();

String wkMsgAction();
bool wkMsgHasAction(const String& action);
bool wkMsgIsFromTimer();
String wkMsgTimerWidgetKey();

bool wkMsgHasField(const String& key);
String wkMsgGetString(const String& key);
int wkMsgGetInt(const String& key);

bool wkPublishRaw(const String& json);
/** 立即发布（不延后）：用于识别现拍大图，避免 WAIT/TTS  defer 导致云端超时 */
bool wkPublishRawUrgent(const String& json);
/**
 * 仅登记设备当前物理状态到本地快照，不发送 MQTT。
 * 连接/重连后的 state.sync 会把该状态投影到当前小程序布局。
 */
bool wkRememberField(const String& key, const String& value);
bool wkPublishField(const String& key, const String& value);
bool wkPublishMatchedField(const String& echoValue);
bool wkPublishMatchedNumber(float number);

void wkSensorClear();
void wkSensorAddNumber(const String& key, float value);
void wkSensorAddInt(const String& key, int value);
void wkSensorAddString(const String& key, const String& value);
void wkSensorAddBool(const String& key, bool value);
String wkSensorJson();
void wkVoiceSetSensors();
void wkVoiceClearSensors();

/* ---------- BLE（原子配置，须先 set* 再 begin） ---------- */
void wkBleSetName(const char* deviceName);
void wkBleSetServiceUuid(const char* serviceUuid);
void wkBleSetWriteUuid(const char* writeUuid);
void wkBleSetNotifyUuid(const char* notifyUuid);
/** 兼容旧积木：一次写入名称与三项 UUID（空串跳过） */
void wkBleSet(const char* deviceName,
              const char* serviceUuid,
              const char* writeUuid,
              const char* notifyUuid);
bool wkBleBegin();
void wkBleLoop();
bool wkBleConnected();
bool wkBleTakeMessage();
String wkBleMsgRaw();
bool wkBleHasField(const String& key);
String wkBleGetString(const String& key);
int wkBleGetInt(const String& key);
bool wkBleMsgMatch(const String& prefix, const String& value);
bool wkBlePrefixHasNumber(const String& prefix, float* outNumber);
String wkBleMatchedKey();
int wkBleMatchedInt();
float wkBleMatchedFloat();
bool wkBlePublishField(const String& key, const String& value);
bool wkBlePublishMatchedField(const String& value);
bool wkBlePublishMatchedNumber(float number);
bool wkBlePublishRaw(const String& json);

/* Internal transport/radio hooks used by the BLE implementation. */
void wkEnableBluetoothWifiCoexistence();
void wkDisableBluetoothWifiCoexistence();
void wkSetBleNetworkExclusive(bool enabled);
bool wkBleNetworkExclusive();
void wkBleSuspendForVoice();
void wkBleResumeAfterVoice();

/* ---------- 扩展：摄像头 / 推流 / 舵机 / 通用下行与语音事件 ---------- */

void wkSetAudioBoard(const char* board); /* "i2s" | "es8311" */
const char* wkGetAudioBoard();
bool wkSetVolume(int percent); /* 0..100，软件输出音量 */
int wkGetVolume();

bool wkCameraBegin();
bool wkCameraOk();
bool wkCameraSnapshotPublish(); /* 抓真帧：MQTT 紧急回传 JPEG+requestId（供语音识别） */

/** 初始化 OLED/彩屏（ST7789）与表情显示。须先 wkCameraBegin。 */
bool wkDisplayBegin();
bool wkDisplayEnabled();
void wkDisplaySetFaceIdle();
void wkDisplaySetFaceListening();
void wkDisplaySetFaceWaiting();
void wkDisplaySetFaceTalking();
void wkDisplaySetReplyText(const char* text);
void wkDisplayClearReplyText();
void wkDisplayOnStreamStart(); /* App 推流：pause 本地取帧；说话时会 resume */
void wkDisplayOnStreamStop();

void wkStreamSetUrl(const char* url);
void wkStreamSetWidgetKey(const char* key);
bool wkStreamStart(); /* 仅 App camera_live_preview=true 时调用 */
void wkStreamStop();  /* App 关预览 */
bool wkStreamIsActive();
/** 现拍 JPEG 经 /ws/device-stream 上传一帧（不依赖 App 预览推流） */
bool wkStreamUploadJpegOnce(const uint8_t* jpeg, size_t len);
/* 兼容旧 API：空操作。语音不再借推流（对齐源码） */
void wkStreamListenLcd(bool enable);
void wkStreamVoiceAssist(bool enable);
void wkStreamVoiceAssistStopSoon(uint32_t delay_ms);
void wkStreamVoiceAssistPoll();

/* 舵机：按动作名调度（如 open_recyclable_lid） */
bool wkTrashBegin(); /* 默认引脚：可回收3 / 厨余14 / 有害48 / 其他38 */
bool wkTrashBeginPins(int pinRecyclable, int pinKitchen, int pinHazardous, int pinOther);
void wkTrashApplyLid(const char* actionName);
bool wkTrashBusy();
uint32_t wkTrashLastMotionMs();
/** 调试：四路动作都映射到同一 GPIO；传 -1 恢复各路独立引脚 */
void wkTrashMapAllToPin(int gpio);

/* 通用下行解析 */
bool wkMsgGetBool(const String& key); /* bool / 对象内任一 true / "true"|"on"|"1" */

/* 兼容旧 API（内部转调通用实现） */
bool wkMsgHasCameraPreviewField();
bool wkMsgCameraPreviewOn();
bool wkMsgIsSmartBinResult();
String wkMsgLidAction();

/* 语音 WS 文本事件（type 由云端决定，如 smart-bin） */
void wkNotifyVoiceEvent(const char* type, const char* json); /* 库内部调用 */
bool wkVoiceTakeEvent(const char* type); /* type 空=任意；非空则匹配 */
String wkVoiceEventRaw();
String wkVoiceEventType();

/* 兼容旧名 */
void wkNotifySmartBin(const char* json);
bool wkVoiceTakeSmartBin();
String wkVoiceSmartBinRaw();

/* 通用上报：{"action":"<action>","ok":true,"raw":<json或字符串>} */
bool wkPublishAction(const String& action, const String& rawJson);

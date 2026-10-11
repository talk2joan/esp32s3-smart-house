/*
 * ============================================================
 *  赛博自建房：完整 AI 智能家居系统
 * ============================================================
 *  集成：LED + 风扇 + 蜂鸣器 + DHT11 + 火焰 + 雨水 + MQ2 + PIR + OLED
 *
 *  【手动控制】（小程序按钮 / AI 语音）
 *    led-on / led-off         开关灯
 *    fan-on / fan-off         手动请求 开/关风扇
 *    buzzer-on / buzzer-off   手动请求 开/关蜂鸣器
 *    ran 0~100                调节语音音量
 *    GPIO15 外接按键          按一次切换一次灯
 *
 *  【自动联动】（手动请求与自动条件“或”运算，关闭按钮只取消手动请求）
 *    风扇   = 手动开 | 烟雾异常 | 高温 | 高湿
 *    蜂鸣器 = 手动开 | 烟雾异常 | 火焰
 *
 *  【触发/恢复阈值】（带回差，避免数值抖动反复切换）
 *    高温   ≥35℃ 触发，≤33℃ 恢复        高湿 ≥70% 触发，≤68% 恢复
 *    烟雾   ≥1800 触发，≤1400 恢复（需 MQ2 预热 120 秒完成）
 *    雨水   ≤2000 触发，≥2300 恢复
 *
 *  【OLED 页面】（SSD1306 128×64 I2C 0x3C，SDA=8 SCL=9）
 *    火焰报警页 > 烟雾报警页 > 正常数据页
 *  小程序：导入 miniprogram-layouts/智能家居-布局.txt
 * ============================================================
 */
#include <house_config.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>

/* ---------------- 引脚 ---------------- */
#define LED_PIN        17   // LED，高电平点亮
#define FAN_INA_PIN    10   // 风扇 PWM（20kHz 8bit 通道0）
#define FAN_INB_PIN    11   // 风扇辅助脚，保持 LOW
#define BUZZER_PIN     18   // 蜂鸣器，高电平响
#define DHT_PIN        16   // DHT11 数据
#define FLAME_PIN      7    // 火焰数字输出（1=有火）
#define RAIN_PIN       6    // 雨水模拟输出
#define MQ2_PIN        5    // MQ2 模拟输出
#define PIR_PIN        14   // 人体红外数字输出（1=有人）
// 外接按键 = GPIO15（在 house_config.h 中定义 PIN_BUTTON / BUTTON_DOWN_LEVEL）

/* ---------------- 风扇 PWM ---------------- */
#define PWM_CHANNEL    0
#define PWM_FREQ       20000
#define PWM_RES_BITS   8
#define FAN_ON_DUTY    160

/* ---------------- 触发 / 恢复阈值（按现场实测调整） ---------------- */
#define TEMP_HIGH_ON    35    // 高温触发值
#define TEMP_HIGH_OFF   33    // 高温恢复值
#define HUMI_HIGH_ON    70    // 高湿触发值
#define HUMI_HIGH_OFF   68    // 高湿恢复值
#define MQ2_ON          1800  // 烟雾触发值
#define MQ2_OFF         1400  // 烟雾恢复值
#define RAIN_ON         2000  // 雨水触发值（≤）
#define RAIN_OFF        2300  // 雨水恢复值（≥）
#define MQ2_WARMUP_MS   120000UL  // MQ2 预热 120 秒

/* ---------------- 周期 ---------------- */
#define SENSOR_PERIOD_MS  1000   // 普通传感器采集
#define DHT_PERIOD_MS     2500   // 温湿度采集
#define UPLOAD_PERIOD_MS  1000   // 小程序数据上报
#define OLED_PERIOD_MS    500    // OLED 刷新
#define DEBOUNCE_MS       200    // 按键消抖

/* ---------------- 对象 ---------------- */
DHT dht(DHT_PIN, DHT11);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/* ---------------- 状态变量 ---------------- */
float temperature = 0, humidity = 0;
int rain_value = 0, mq2_value = 0;
int flame_detected = 0, rain_detected = 0, smoke_detected = 0, human_detected = 0;
int high_temperature = 0, high_humidity = 0, mq2_ready = 0;
int led_on = 0, fan_manual = 0, buzzer_manual = 0, fan_on = 0, buzzer_on = 0;
String flame_status, rain_status, mq2_status, human_status;

uint32_t system_start_time = 0;
uint32_t last_sensor_time = 0, last_dht_time = 0, last_upload_time = 0;
uint32_t last_oled_time = 0, last_button_time = 0;
int button_now = 0, button_last = 0;

/* ================= 初始化 ================= */
void setup() {
  /* 输出引脚初始化：全部默认关闭 */
  pinMode(LED_PIN, OUTPUT);    digitalWrite(LED_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(FAN_INB_PIN, OUTPUT); digitalWrite(FAN_INB_PIN, LOW);
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES_BITS);
  ledcAttachPin(FAN_INA_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);

  /* 输入引脚 */
  pinMode(FLAME_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(PIN_BUTTON, INPUT);

  /* DHT11 与 OLED */
  dht.begin();
  Wire.begin(8, 9);            // SDA=GPIO8, SCL=GPIO9
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  system_start_time = millis();  // 启动 MQ2 预热计时

  wkHouseInit();                // WiFi + MQTT + AI 语音（音量初始 75）

  Serial.println("[智能家居] 完整系统已启动");
}

/* ================= 传感器采集 ================= */
void readSensors() {
  /* DHT11：每 2.5 秒 */
  if (millis() - last_dht_time >= DHT_PERIOD_MS) {
    last_dht_time = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) { temperature = t; humidity = h; }

    /* 高温 / 高湿（带回差） */
    if (temperature >= TEMP_HIGH_ON) high_temperature = 1;
    else if (temperature <= TEMP_HIGH_OFF) high_temperature = 0;
    if (humidity >= HUMI_HIGH_ON) high_humidity = 1;
    else if (humidity <= HUMI_HIGH_OFF) high_humidity = 0;
  }

  /* 其余传感器：每 1 秒 */
  if (millis() - last_sensor_time >= SENSOR_PERIOD_MS) {
    last_sensor_time = millis();

    /* MQ2 预热完成后才允许触发烟雾报警 */
    mq2_ready = (millis() - system_start_time >= MQ2_WARMUP_MS) ? 1 : 0;
    mq2_value = analogRead(MQ2_PIN);
    if (mq2_ready) {
      if (mq2_value >= MQ2_ON) smoke_detected = 1;
      else if (mq2_value <= MQ2_OFF) smoke_detected = 0;
      mq2_status = smoke_detected ? "异常" : "空气状态正常";
    } else {
      smoke_detected = 0;
      mq2_status = "传感器预热中";
    }

    /* 雨水：数值低=湿润（带回差） */
    rain_value = analogRead(RAIN_PIN);
    if (rain_value <= RAIN_ON) rain_detected = 1;
    else if (rain_value >= RAIN_OFF) rain_detected = 0;
    rain_status = rain_detected ? "检测到雨水" : "探头干燥";

    /* 火焰 / 人体：直接读取 */
    flame_detected = digitalRead(FLAME_PIN);
    flame_status = flame_detected ? "检测到火焰" : "未检测到火焰";
    human_detected = digitalRead(PIR_PIN);
    human_status = human_detected ? "检测到人" : "未检测到人";
  }
}

/* ================= 外接按键：按一次切换灯 ================= */
void readButton() {
  button_now = (digitalRead(PIN_BUTTON) == BUTTON_DOWN_LEVEL) ? 1 : 0;
  if (button_now == 1 && button_last == 0 && millis() - last_button_time >= DEBOUNCE_MS) {
    last_button_time = millis();
    led_on = !led_on;
    digitalWrite(LED_PIN, led_on ? HIGH : LOW);
  }
  button_last = button_now;
}

/* ================= 设备状态计算与引脚输出 ================= */
void applyOutputs() {
  /* 风扇：手动开 | 烟雾 | 高温 | 高湿 */
  int new_fan = (fan_manual || smoke_detected || high_temperature || high_humidity) ? 1 : 0;
  if (new_fan != fan_on) {
    fan_on = new_fan;
    ledcWrite(PWM_CHANNEL, fan_on ? FAN_ON_DUTY : 0);
    digitalWrite(FAN_INB_PIN, LOW);
  }
  /* 蜂鸣器：手动开 | 烟雾 | 火焰 */
  int new_buz = (buzzer_manual || smoke_detected || flame_detected) ? 1 : 0;
  if (new_buz != buzzer_on) {
    buzzer_on = new_buz;
    digitalWrite(BUZZER_PIN, buzzer_on ? HIGH : LOW);
  }
}

/* ================= 每 1 秒上报小程序页面数据 ================= */
void uploadFields() {
  if (millis() - last_upload_time < UPLOAD_PERIOD_MS) return;
  last_upload_time = millis();

  wkPublishField("temperature", String(temperature));
  wkPublishField("humidity", String(humidity));
  wkPublishField("mq2_value", String(mq2_value));
  wkPublishField("mq2_status", mq2_status);
  wkPublishField("rain_value", String(rain_value));
  wkPublishField("rain_status", rain_status);
  wkPublishField("flame_detected", String(flame_detected));
  wkPublishField("flame_status", flame_status);
  wkPublishField("human_detected", String(human_detected));
  wkPublishField("human_status", human_status);

  wkPublishField("led_status", led_on ? "灯已打开" : "灯已关闭");
  wkPublishField("fan_status", fan_on ? "风扇已打开" : "风扇已关闭");
  wkPublishField("buzzer_status", buzzer_on ? "蜂鸣器已打开" : "蜂鸣器已关闭");

  /* 综合报警：火焰 > 烟雾 > 高温 > 高湿 > 正常 */
  if (flame_detected)            wkPublishField("alarm_status", "火焰报警，蜂鸣器开启");
  else if (smoke_detected)       wkPublishField("alarm_status", "烟雾报警，风扇和蜂鸣器开启");
  else if (high_temperature)     wkPublishField("alarm_status", "高温报警，风扇开启");
  else if (high_humidity)        wkPublishField("alarm_status", "湿度过高，风扇开启");
  else                           wkPublishField("alarm_status", "正常");

  wkPublishField("temp_status", high_temperature ? "温度较高" : "温度正常");
  wkPublishField("humi_status", high_humidity ? "湿度较高" : "湿度正常");
}

/* ================= 处理小程序 / AI 下行命令 ================= */
void handleMessages() {
  if (!wkTakeMessage()) return;

  if (wkMsgMatch("btn", "led-on")) {          // 开灯
    led_on = 1; digitalWrite(LED_PIN, HIGH);
  } else if (wkMsgMatch("btn", "led-off")) {  // 关灯
    led_on = 0; digitalWrite(LED_PIN, LOW);
  } else if (wkMsgMatch("btn", "fan-on")) {   // 手动开风扇
    fan_manual = 1;
  } else if (wkMsgMatch("btn", "fan-off")) {  // 取消手动风扇
    fan_manual = 0;
  } else if (wkMsgMatch("btn", "buzzer-on")) {// 手动开蜂鸣器
    buzzer_manual = 1;
  } else if (wkMsgMatch("btn", "buzzer-off")){// 取消手动蜂鸣器
    buzzer_manual = 0;
  } else {
    wkHouseHandleVolume();                    // ran → 音量
  }
  wkFinishMessage();
}

/* ================= OLED 页面（火焰 > 烟雾 > 正常） ================= */
void refreshOled() {
  if (millis() - last_oled_time < OLED_PERIOD_MS) return;
  last_oled_time = millis();

  u8g2.firstPage();
  do {
    if (flame_detected) {
      /* 火焰报警页 */
      u8g2.setFont(u8g2_font_ncenB14_tr);
      u8g2.drawStr(4, 20, "FIRE ALARM");
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(4, 42, "FLAME: 1");
      u8g2.drawStr(4, 60, "BUZZER: ON");
    } else if (smoke_detected) {
      /* 烟雾报警页 */
      u8g2.setFont(u8g2_font_ncenB14_tr);
      u8g2.drawStr(4, 20, "SMOKE ALARM");
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(4, 42);  u8g2.print("MQ2: "); u8g2.print(mq2_value);
      u8g2.drawStr(4, 60, "FAN: ON  BUZZER: ON");
    } else {
      /* 正常数据页：T/H，MQ2/PIR，Rain/LED，Fire/BUZ */
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 14);  u8g2.print("T:");  u8g2.print(temperature, 1); u8g2.print("C");
      u8g2.setCursor(70, 14); u8g2.print("H:");  u8g2.print(humidity, 1);    u8g2.print("%");
      u8g2.setCursor(0, 30);
      if (mq2_ready) u8g2.print("MQ2:" + String(mq2_value));
      else           u8g2.print("MQ2:WARM");
      u8g2.setCursor(70, 30); u8g2.print("PIR:"); u8g2.print(human_detected);
      u8g2.setCursor(0, 46);  u8g2.print("Rain:"); u8g2.print(rain_value);
      u8g2.setCursor(70, 46); u8g2.print("LED:");  u8g2.print(led_on);
      u8g2.setCursor(0, 62);  u8g2.print("Fire:"); u8g2.print(flame_detected);
      u8g2.setCursor(70, 62); u8g2.print("BUZ:");  u8g2.print(buzzer_on);
    }
  } while (u8g2.nextPage());
}

/* ================= 主循环 ================= */
void loop() {
  wkHouseLoop();    // 1. 网络、语音与语音库按键处理
  readButton();      // 2. 外接按键读取（切换灯）
  readSensors();     // 3. 温湿度与其他传感器采集
  applyOutputs();    // 4. 设备状态计算与引脚输出
  uploadFields();    // 5. 向小程序上报数据
  handleMessages();  // 6. 处理小程序控制命令
  refreshOled();     // 7. OLED 页面刷新

  /* 8. 向 AI 提供当前环境与设备状态 */
  wkSensorClear();
  wkSensorAddNumber("volume", wkGetVolume());
  wkSensorAddNumber("temperature", temperature);
  wkSensorAddNumber("humidity", humidity);
  wkSensorAddNumber("flame", flame_detected);
  wkSensorAddString("flame_status", flame_status);
  wkSensorAddNumber("rain", rain_value);
  wkSensorAddString("rain_status", rain_status);
  wkSensorAddNumber("mq2", mq2_value);
  wkSensorAddString("mq2_status", mq2_status);
  wkSensorAddNumber("human", human_detected);
  wkSensorAddString("human_status", human_status);
  wkSensorAddBool("led_on", led_on);
  wkSensorAddBool("fan_on", fan_on);
  wkSensorAddBool("buzzer_on", buzzer_on);
  wkVoiceSetSensors();
}

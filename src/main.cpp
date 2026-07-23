/**
 * @file main.cpp
 * @brief 教室环境监测 - 主程序
 *
 * 主界面: 显示光照/空气/温度/湿度（中文+进度条+入场动画）
 * 设置:   长按进入，旋转调节阈值，短按确认，长按返回
 * 串口:   每2秒打印传感器数据(115200)
 *
 * 引脚:   OLED(SDA=17,SCL=18) 编码器(CLK=4,DT=5,SW=8)
 *         光敏=7 MQ135=15 DHT11=16
 */

#include <Arduino.h>
#include "sensors.h"
#include "SSD1306.h"

// ==================== 编码器引脚 ====================
#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  8

// ==================== 全局对象 ====================
static SensorManager sensors;

// ==================== 编码器 ====================
static volatile int encDelta = 0;
static volatile int encCount = 0;
static uint32_t lastIsrTime = 0;

static void IRAM_ATTR encISR() {
  uint32_t now = micros();
  if (now - lastIsrTime < 500) return;
  lastIsrTime = now;
  if (digitalRead(ENCODER_DT)) encCount++;
  else encCount--;
  if (encCount >= 4) { encDelta++; encCount = 0; }
  if (encCount <= -4) { encDelta--; encCount = 0; }
}

// 读取并清零编码器增量（原子操作）
static int readEncoder() {
  noInterrupts();
  int d = encDelta;
  encDelta = 0;
  interrupts();
  return d;
}

// ==================== 按钮 ====================
static unsigned long btnDownTime = 0;
static bool btnWasDown = false;
static bool shortPress = false;
static bool longPress = false;

// 短按(<500ms) / 长按(>=500ms) 检测
static void updateButton() {
  shortPress = false;
  longPress = false;
  bool down = digitalRead(ENCODER_SW) == HIGH;
  if (down && !btnWasDown) { btnDownTime = millis(); btnWasDown = true; }
  if (!down && btnWasDown) {
    btnWasDown = false;
    if (millis() - btnDownTime < 300) shortPress = true;
    else longPress = true;
  }
}

// ==================== 状态 ====================
static enum Mode { MODE_MONITOR, MODE_SETTINGS } mode = MODE_MONITOR;

static float thresholds[4] = {50, 500, 30, 60}; // 光照/空气(PPM)/温度/湿度阈值
static int settingsIndex = 0;                   // 当前选中的阈值
static bool editing = false;                    // 是否在编辑模式

// ==================== setup ====================
void setup() {
  Serial.begin(115200);

  // OLED
  oledInit();
  drawBootScreen();
  delay(1500);

  // 编码器：上拉输入 + RISING沿中断
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encISR, RISING);

  sensors.begin();
}

// ==================== loop ====================
void loop() {
  sensors.update();
  updateButton();
  int rawDelta = readEncoder();

  int delta = 0;
  if (rawDelta > 0) delta = 1;
  else if (rawDelta < 0) delta = -1;

  if (mode == MODE_MONITOR) {
    // 主界面：长按进入设置
    if (longPress) {
      mode = MODE_SETTINGS;
      settingsIndex = 0;
      editing = false;
    }
    drawMonitor(sensors.getData(), settingsIndex, editing, sensors.isMQ135Ready());
  } else {
    // 设置界面
    if (editing) {
      // 编辑模式：旋转调数值
      if (delta != 0) {
        thresholds[settingsIndex] += static_cast<float>(delta) * (settingsIndex == 1 ? 10.0f : 1.0f);
        float maxVal = (settingsIndex == 1) ? 1000.0f : 100.0f;
        thresholds[settingsIndex] = constrain(thresholds[settingsIndex], 0.0f, maxVal);
      }
      if (shortPress) editing = false;       // 短按确认
    } else {
      // 选择模式：旋转切项
      if (delta > 0 && settingsIndex < 3) settingsIndex++;
      if (delta < 0 && settingsIndex > 0) settingsIndex--;
      if (shortPress) editing = true;        // 短按进入编辑
      if (longPress) mode = MODE_MONITOR;    // 长按返回主界面
    }
    drawSettings(thresholds, settingsIndex, editing);
  }

  delay(15);
}

/**
 * @file sensors.cpp
 * @brief 传感器管理器实现
 */

#include "sensors.h"

SensorManager::SensorManager()
  : dht(PIN_DHT11, DHT_TYPE), lastUpdate(0), mq135Ready(false), mq135StartTime(0) {
  data = {0, 0, 0, 0};
}

void SensorManager::begin() {
  dht.begin();
  pinMode(PIN_LIGHT, INPUT);
  pinMode(PIN_MQ135, INPUT);
  mq135StartTime = millis();
  Serial.println("[Sensors] OK (MQ135 warming up 15s)");
}

void SensorManager::update() {
  // 每2秒读取一次，避免频繁读硬件
  if (millis() - lastUpdate < 2000) return;
  lastUpdate = millis();

  // 光敏: ADC 0~4095 -> 百分比（值越高越亮，需要反转）
  data.light = 100.0f - (analogRead(PIN_LIGHT) / 4095.0f * 100.0f);

  // MQ135: 需要15秒预热
  if (!mq135Ready) {
    if (millis() - mq135StartTime >= 15000) {
      mq135Ready = true;
      Serial.println("[Sensors] MQ135 warmup complete");
    }
  }
  if (mq135Ready) {
    // MQ135: ADC 0~4095 -> PPM (粗略映射)
    data.aqi = analogRead(PIN_MQ135) / 4095.0f * 1000.0f;
  }

  // DHT11: 读失败保留上次值
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) data.temp = t;
  if (!isnan(h)) data.humid = h;
}

SensorData SensorManager::getData() { return data; }
bool SensorManager::isMQ135Ready() { return mq135Ready; }
String SensorManager::lightStr() { return String(data.light, 1) + "%"; }
String SensorManager::aqiStr()   { return String(data.aqi, 0) + "ppm"; }
String SensorManager::tempStr()  { return String(data.temp, 1) + "C"; }
String SensorManager::humidStr() { return String(data.humid, 1) + "%"; }

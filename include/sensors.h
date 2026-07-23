/**
 * @file sensors.h
 * @brief 传感器管理器 - 读取光敏/MQ135/DHT11，每2秒更新一次
 */

#pragma once
#include <Arduino.h>
#include <DHT.h>

#define PIN_LIGHT   7       // 光敏电阻 ADC
#define PIN_MQ135   15      // MQ135 ADC
#define PIN_DHT11   16      // DHT11 数据引脚
#define DHT_TYPE    DHT11

struct SensorData {
  float light;    // 光照 0~100%
  float aqi;      // 空气质量 PPM
  float temp;     // 温度 °C
  float humid;    // 湿度 %
};

class SensorManager {
public:
  SensorManager();
  void begin();               // 初始化传感器
  void update();              // 每2秒读取一次
  SensorData getData();
  bool isMQ135Ready();
  String lightStr();          // "65.2%"
  String aqiStr();
  String tempStr();           // "25.6C"
  String humidStr();
private:
  DHT dht;
  SensorData data;
  unsigned long lastUpdate;
  bool mq135Ready;
  unsigned long mq135StartTime;
};

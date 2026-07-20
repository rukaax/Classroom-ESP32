/**
 * @file sensors.cpp
 * @brief 传感器管理器实现
 * @brief 负责读取光敏电阻、MQ135、DHT11三种传感器，并缓存数据供显示使用
 */

#include "sensors.h"

// ==================== 构造函数 ====================
/**
 * @brief 初始化传感器管理器
 * @param interval 数据更新间隔（毫秒），默认2000ms
 *
 * 注意：此处只是设置参数，实际硬件初始化在begin()中完成
 */
SensorManager::SensorManager(unsigned long interval)
    : dht(PIN_DHT11, DHT_TYPE),   // 初始化DHT11对象，指定引脚和类型
      updateInterval(interval) {   // 保存更新间隔
  // 将数据结构体清零
  data = {0, 0, 0, 0, 0};
}

// ==================== 初始化 ====================
/**
 * @brief 初始化所有传感器硬件（在setup()中调用一次）
 *
 * 初始化内容：
 *   1. DHT11传感器启动（内部会等待传感器就绪）
 *   2. 光敏电阻引脚配置为输入模式（ADC读取）
 *   3. MQ135引脚配置为输入模式（ADC读取）
 */
void SensorManager::begin() {
  // dht.begin();  // DHT11未接时会阻塞，确认接好后再启用
  pinMode(PIN_LIGHT, INPUT);      // 光敏电阻引脚设为输入
  pinMode(PIN_MQ135, INPUT);      // MQ135引脚设为输入
  Serial.println("[Sensors] Sensors initialized.");
}

// ==================== 数据更新 ====================
/**
 * @brief 更新传感器数据（在loop()中每帧调用）
 *
 * 工作流程：
 *   1. 检查是否到达更新间隔（默认2秒），未到则直接返回
 *   2. 读取光敏电阻ADC值，转换为百分比（0~100%）
 *   3. 读取MQ135 ADC值，转换为百分比（0~100%）
 *   4. 读取DHT11温度和湿度（如果读取失败则保留上次的值）
 *
 * ESP32的ADC为12位（0~4095），3.3V参考电压
 * 光照百分比 = ADC值 / 4095 * 100
 * 空气质量百分比 = ADC值 / 4095 * 100
 */
void SensorManager::update() {
  unsigned long now = millis();

  // 未到达更新间隔，直接返回（避免频繁读取硬件）
  if (now - data.lastUpdate < updateInterval) return;
  data.lastUpdate = now;  // 记录本次更新时间

  // ---------- 读取光敏电阻 ----------
  // analogRead()返回0~4095（ESP32 12位ADC）
  // 转换为百分比：0% = 全暗，100% = 全亮
  int rawLight = analogRead(PIN_LIGHT);
  data.lightLevel = rawLight / 4095.0f * 100.0f;

  // ---------- 读取MQ135气体传感器 ----------
  // MQ135输出模拟电压，ADC读取后转换为百分比
  // 注意：实际气体浓度需要根据传感器校准曲线换算，这里简化为百分比显示
  int rawMQ = analogRead(PIN_MQ135);
  data.mq135Value = rawMQ / 4095.0f * 100.0f;

  // ---------- 读取DHT11温湿度传感器 ----------
  // 注意：如果DHT11未接线或无响应，expectPulse()会阻塞导致WDT超时
  // 确认DHT11已正确接线后，取消下面两行的注释即可启用
  // float t = dht.readTemperature();
  // float h = dht.readHumidity();
  // if (!isnan(t)) data.temperature = t;
  // if (!isnan(h)) data.humidity = h;
}

// ==================== 数据获取 ====================
/**
 * @brief 获取缓存的传感器数据（不会触发新的硬件读取）
 * @return 包含所有传感器最新读数的结构体
 */
SensorData SensorManager::getData() {
  return data;
}

// ==================== 格式化输出 ====================
/**
 * @brief 获取光照百分比字符串，格式如 "65.2%"
 */
String SensorManager::getLightString() {
  return String(data.lightLevel, 1) + "%";  // 保留1位小数
}

/**
 * @brief 获取空气质量百分比字符串，格式如 "23.1%"
 */
String SensorManager::getMQ135String() {
  return String(data.mq135Value, 1) + "%";  // 保留1位小数
}

/**
 * @brief 获取温度字符串，格式如 "25.6C"
 */
String SensorManager::getTempString() {
  return String(data.temperature, 1) + "C";  // 保留1位小数
}

/**
 * @brief 获取湿度百分比字符串，格式如 "60.3%"
 */
String SensorManager::getHumidityString() {
  return String(data.humidity, 1) + "%";  // 保留1位小数
}

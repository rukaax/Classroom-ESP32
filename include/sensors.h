/**
 * @file sensors.h
 * @brief 传感器管理器头文件
 * @brief 负责读取并管理三种传感器的数据：
 *   - 光敏电阻（接ADC引脚，读取光照强度百分比）
 *   - MQ135气体传感器（接ADC引脚，读取空气质量百分比）
 *   - DHT11温湿度传感器（单总线数字接口，读取温度和湿度）
 */

#pragma once
#ifndef SENSORS_H_
#define SENSORS_H_

#include <Arduino.h>
#include <DHT.h>        // DHT11温湿度传感器库（adafruit/DHT sensor library）

// ==================== 传感器引脚定义 ====================
// 注意：这些引脚与 hal_esp32.h 中的定义保持一致
#define PIN_LIGHT   7     // 光敏电阻模拟输入引脚（ADC通道）
#define PIN_MQ135   15    // MQ135气体传感器模拟输入引脚（ADC通道）
#define PIN_DHT11   16    // DHT11温湿度传感器数据引脚（单总线）
#define DHT_TYPE    DHT11 // 传感器类型：DHT11（还有DHT22可选）

// ==================== 传感器数据结构体 ====================
/**
 * @brief 存储所有传感器的最新读数
 */
struct SensorData {
  float lightLevel;       // 光照强度（0~100%）
  float mq135Value;       // 空气质量（0~100%，值越高空气质量越好/越差取决于校准）
  float temperature;      // 温度（摄氏度）
  float humidity;         // 相对湿度（0~100%）
  unsigned long lastUpdate; // 上次更新时间戳（毫秒）
};

// ==================== 传感器管理器类 ====================
/**
 * @brief 统一管理所有传感器的读取和数据缓存
 *
 * 使用方法：
 *   SensorManager sensors(2000);  // 创建管理器，每2000ms更新一次
 *   sensors.begin();              // 在setup()中初始化
 *   sensors.update();             // 在loop()中调用，内部自动控制读取频率
 *   SensorData data = sensors.getData();  // 获取最新数据
 */
class SensorManager {
private:
  DHT dht;                // DHT11传感器对象
  SensorData data;        // 缓存的传感器数据
  unsigned long updateInterval; // 数据更新间隔（毫秒）

public:
  /**
   * @brief 构造函数
   * @param interval 数据更新间隔（毫秒），默认2000ms
   */
  SensorManager(unsigned long interval = 2000);

  /**
   * @brief 初始化所有传感器（在setup()中调用）
   *        - 启动DHT11
   *        - 配置光敏电阻和MQ135的ADC引脚
   */
  void begin();

  /**
   * @brief 更新传感器数据（在loop()中调用）
   *        内部会判断是否到达更新间隔，未到则直接返回
   *        到达间隔后依次读取：光敏电阻 -> MQ135 -> DHT11
   */
  void update();

  /**
   * @brief 获取缓存的传感器数据
   * @return SensorData 结构体，包含所有传感器的最新读数
   */
  SensorData getData();

  // ==================== 格式化输出方法 ====================
  // 以下方法将传感器数据格式化为字符串，用于串口输出和屏幕显示

  String getLightString();     // 返回 "65.2%" 格式的光照百分比
  String getMQ135String();     // 返回 "23.1%" 格式的空气质量百分比
  String getTempString();      // 返回 "25.6C" 格式的温度
  String getHumidityString();  // 返回 "60.3%" 格式的湿度百分比
};

#endif

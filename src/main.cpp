/**
 * @file main.cpp
 * @brief 教室环境监测系统 - 主程序
 * @brief 基于 ESP32-S3 N16R8 + oled-ui-astra UI框架
 *
 * 功能概述：
 *   1. 通过SSD1306 OLED屏幕(I2C)显示菜单和传感器数据
 *   2. 旋转编码器控制菜单导航（左转=上一项，右转=下一项，按下=确认/返回）
 *   3. 读取光敏电阻、MQ135气体传感器、DHT11温湿度传感器
 *   4. 传感器数据实时显示在OLED屏幕和串口上（串口波特率115200）
 */

#include <Arduino.h>
#include "hal/hal_esp32.h"           // ESP32硬件抽象层（OLED驱动、编码器、系统延时等）
#include "astra/ui/launcher.h"       // astra UI启动器（管理菜单树、摄像机、选择器）
#include "astra/astra_logo.h"        // astra UI开机动画
#include "sensors.h"                 // 传感器管理器（光敏、MQ135、DHT11）

// ==================== 全局对象 ====================

HALESP32* halESP32 = nullptr;        // ESP32硬件抽象层实例，负责驱动OLED和读取编码器
astra::Launcher* astraLauncher = nullptr;  // astra UI启动器，负责管理整个菜单系统
astra::Menu* rootPage = nullptr;     // 根菜单页面
astra::Menu* sensorPage = nullptr;   // 传感器子菜单页面

SensorManager sensors(2000);         // 传感器管理器，每2000ms更新一次数据

// ==================== 串口打印相关 ====================

unsigned long lastSerialPrint = 0;   // 上次串口打印的时间戳（毫秒）
const unsigned long SERIAL_INTERVAL = 2000;  // 串口打印间隔：2秒

// ==================== 菜单树构建 ====================
/**
 * @brief 构建astra UI菜单树
 *
 * 菜单结构：
 *   Classroom（根菜单）
 *     └── Sensors（传感器菜单）
 *           ├── Light（光照强度）
 *           ├── MQ135（空气质量）
 *           └── DHT11（温度/湿度）
 */
void buildMenuTree() {
  // 创建根菜单，标题为"Classroom"
  rootPage = new astra::Menu("Classroom");
  // 创建传感器子菜单，标题为"Sensors"
  sensorPage = new astra::Menu("Sensors");

  // 向传感器菜单添加三个子项
  sensorPage->addItem(new astra::Menu("Light"));   // 光照传感器页面
  sensorPage->addItem(new astra::Menu("MQ135"));   // MQ135气体传感器页面
  sensorPage->addItem(new astra::Menu("DHT11"));   // DHT11温湿度传感器页面

  // 将传感器菜单挂载到根菜单下
  rootPage->addItem(sensorPage);

  // 创建启动器并用根菜单初始化（这会建立整个菜单树）
  astraLauncher = new astra::Launcher();
  astraLauncher->init(rootPage);
}

// ==================== 串口数据输出 ====================
/**
 * @brief 将传感器数据格式化输出到串口（115200波特率）
 * @brief 每2秒调用一次，输出格式如下：
 *
 *   ===== Sensor Data =====
 *   Light:  65.2%
 *   MQ135:  23.1%
 *   Temp:   25.6C
 *   Humid:  60.3%
 *   =======================
 */
void printSensorDataToSerial() {
  SensorData d = sensors.getData();

  Serial.println("===== Sensor Data =====");
  Serial.print("Light:  "); Serial.println(sensors.getLightString());   // 光照百分比
  Serial.print("MQ135:  "); Serial.println(sensors.getMQ135String());   // 空气质量百分比
  Serial.print("Temp:   "); Serial.println(sensors.getTempString());    // 温度（摄氏度）
  Serial.print("Humid:  "); Serial.println(sensors.getHumidityString()); // 湿度百分比
  Serial.println("=======================");
}

// ==================== Arduino setup() ====================
/**
 * @brief 系统初始化（上电后执行一次）
 *
 * 初始化顺序：
 *   1. 注入ESP32硬件抽象层（初始化OLED I2C、编码器中断、串口）
 *   2. 初始化传感器（DHT11、ADC引脚）
 *   3. 播放开机动画
 *   4. 构建菜单树
 */
void setup() {
  // 创建ESP32硬件抽象层实例并注入到HAL系统
  // 这会自动调用 HALESP32::init()，完成：
  //   - 串口初始化（115200）
  //   - I2C总线初始化（SDA=GPIO17, SCL=GPIO18）
  //   - SSD1306 OLED显示屏初始化（128x64, I2C地址0x3C）
  //   - 旋转编码器初始化（CLK=GPIO4, DT=GPIO5, SW=GPIO6，带中断）
  halESP32 = new HALESP32();
  HAL::inject(halESP32);

  // 初始化传感器（DHT11启动、ADC引脚配置）
  sensors.begin();

  // 短暂延时后播放astra UI开机动画（星星+文字淡入效果，持续800帧）
  HAL::delay(200);
  astra::drawLogo(800);

  // 构建菜单树（Classroom -> Sensors -> Light/MQ135/DHT11）
  buildMenuTree();

  Serial.println("[Main] 系统就绪。");
}

// ==================== Arduino loop() ====================
/**
 * @brief 主循环（重复执行）
 *
 * 每次循环做的事情：
 *   1. 更新传感器数据（内部有2秒间隔控制，不会每次都读取硬件）
 *   2. 更新astra UI（渲染菜单、处理编码器输入、移动摄像机和选择器）
 *   3. 每2秒将传感器数据输出到串口
 */
void loop() {
  // 更新传感器读数（光敏、MQ135、DHT11）
  // 内部会判断是否到达2秒间隔，未到则直接返回，不影响帧率
  sensors.update();

  // 更新astra UI系统
  // 内部执行：清屏 -> 渲染菜单 -> 渲染选择器 -> 更新摄像机位置 -> 处理按键 -> 刷屏
  // 旋转编码器的左转/右转/按下会被自动处理为菜单导航操作
  astraLauncher->update();

  // 定时将传感器数据输出到串口（每2秒一次）
  unsigned long now = millis();
  if (now - lastSerialPrint >= SERIAL_INTERVAL) {
    lastSerialPrint = now;
    printSensorDataToSerial();
  }
}

/**
 * @file hal_esp32.h
 * @brief ESP32硬件抽象层（HAL）头文件
 * @brief 继承自astra UI的HAL基类，将所有硬件操作桥接到ESP32 Arduino框架
 *
 * 职责：
 *   1. 驱动SSD1306 OLED显示屏（128x64, I2C接口，通过U8g2库）
 *   2. 读取旋转编码器（CLK/DT/SW三线制，带硬件中断）
 *   3. 提供系统延时、毫秒计时、随机种子等基础功能
 *
 * 引脚分配：
 *   OLED:  SDA=GPIO17, SCL=GPIO18, 地址=0x3C
 *   编码器: CLK=GPIO4, DT=GPIO5, SW=GPIO6
 */

#pragma once
#ifndef HAL_ESP32_H_
#define HAL_ESP32_H_

#include "hal.h"            // astra UI硬件抽象层基类
#include <Arduino.h>        // Arduino框架核心
#include <U8g2lib.h>        // U8g2图形库（用于驱动SSD1306）
#include <Wire.h>           // I2C通信库

// ==================== 引脚定义 ====================

#define OLED_SDA  17        // OLED I2C数据线
#define OLED_SCL  18        // OLED I2C时钟线
#define OLED_ADDR 0x3C      // OLED I2C设备地址（SSD1306默认地址）

#define ENCODER_CLK 6       // 旋转编码器时钟引脚（A相）
#define ENCODER_DT  4       // 旋转编码器数据引脚（B相）
#define ENCODER_SW  5       // 旋转编码器按钮引脚（按下接地）

#define PIN_LIGHT   7       // 光敏电阻ADC引脚（与sensors.h保持一致）
#define PIN_MQ135   15      // MQ135 ADC引脚（与sensors.h保持一致）
#define PIN_DHT11   16      // DHT11数据引脚（与sensors.h保持一致）

// ==================== ESP32 HAL类 ====================
/**
 * @brief ESP32平台的HAL实现
 *
 * 继承自HAL基类，重写所有虚函数，将astra UI的硬件调用
 * 映射到ESP32 Arduino框架的具体实现：
 *
 *   绘图函数 -> U8g2库的对应函数
 *   延时函数 -> Arduino的delay()/millis()
 *   按键函数 -> 旋转编码器的中断读取
 *
 * 使用方法：
 *   halESP32 = new HALESP32();   // 创建实例
 *   HAL::inject(halESP32);       // 注入到HAL系统（自动调用init()）
 */
class HALESP32 : public HAL {
private:
  u8g2_t u8g2;                    // U8g2显示驱动实例（C语言结构体）
  volatile int encoderPos;        // 编码器位置增量（由ISR修改，volatile防止编译器优化）
  volatile bool encoderButtonPressed; // 编码器按钮是否被按下
  unsigned long lastEncoderTick;  // 上次编码器中断时间戳（用于消抖）

public:
  HALESP32() = default;           // 默认构造函数

  // ==================== 初始化 ====================
  /**
   * @brief 初始化所有硬件（在HAL::inject()时自动调用）
   *
   * 初始化内容：
   *   1. 串口初始化（115200波特率）
   *   2. I2C总线初始化（SDA/SCL引脚）
   *   3. SSD1306 OLED初始化（128x64全缓冲模式）
   *   4. 旋转编码器初始化（引脚配置 + 硬件中断注册）
   */
  void init() override;

  // ==================== OLED绘图接口 ====================
  // 以下函数将astra UI的绘图调用映射到U8g2库
  // astra UI通过这些函数在OLED上绘制菜单、选择框、文字等

  void* _getCanvasBuffer() override;           // 获取画布缓冲区指针
  uint8_t _getBufferTileHeight() override;     // 获取画布高度（tile单位）
  uint8_t _getBufferTileWidth() override;      // 获取画布宽度（tile单位）
  void _canvasUpdate() override;               // 将画布内容刷新到OLED屏幕
  void _canvasClear() override;                // 清空画布（全黑）
  void _setFont(const uint8_t* _font) override;  // 设置当前字体
  uint8_t _getFontWidth(std::string& _text) override;  // 计算文字像素宽度
  uint8_t _getFontHeight() override;           // 获取当前字体高度（像素）
  void _setDrawType(uint8_t _type) override;   // 设置绘图颜色模式（0=黑, 1=白, 2=反色）
  void _drawPixel(float _x, float _y) override;       // 画单个像素
  void _drawEnglish(float _x, float _y, const std::string& _text) override;  // 绘制英文/UTF8文字
  void _drawChinese(float _x, float _y, const std::string& _text) override;  // 绘制中文文字
  void _drawVDottedLine(float _x, float _y, float _h) override;  // 画垂直虚线
  void _drawHDottedLine(float _x, float _y, float _l) override;  // 画水平虚线
  void _drawVLine(float _x, float _y, float _h) override;        // 画垂直实线
  void _drawHLine(float _x, float _y, float _l) override;        // 画水平实线
  void _drawBMP(float _x, float _y, float _w, float _h, const uint8_t* _bitMap) override;  // 画位图
  void _drawBox(float _x, float _y, float _w, float _h) override;       // 画填充矩形
  void _drawRBox(float _x, float _y, float _w, float _h, float _r) override;  // 画填充圆角矩形
  void _drawFrame(float _x, float _y, float _w, float _h) override;     // 画空心矩形
  void _drawRFrame(float _x, float _y, float _w, float _h, float _r) override; // 画空心圆角矩形

  // ==================== 系统时间接口 ====================
  void _delay(unsigned long _mill) override;    // 延时（毫秒）
  unsigned long _millis() override;             // 获取开机以来的毫秒数
  unsigned long _getTick() override;            // 获取系统节拍（与millis相同）
  unsigned long _getRandomSeed() override;      // 获取随机种子（基于ADC噪声）

  // ==================== 按键接口 ====================
  // astra UI的按键系统有两个键：KEY_0和KEY_1
  // 我们将旋转编码器映射为：
  //   KEY_0 = 左转（上一项）
  //   KEY_1 = 右转（下一项）或 按下按钮（确认/进入）
  //
  // astra UI的keyScan()状态机处理消抖和长短按判断：
  //   短按 -> CLICK（用于菜单项切换）
  //   长按 -> PRESS（用于进入/返回子菜单）

  bool _getKey(key::KEY_INDEX _keyIndex) override;  // 检查指定键是否被按下
  void _keyScan() override;                          // 按键扫描（带消抖状态机）

  // ==================== 编码器辅助方法 ====================
  /**
   * @brief 获取编码器增量值（正=右转，负=左转，0=无转动）
   * @note 读取后会自动清零
   */
  int getEncoderDelta();

  /**
   * @brief 检查编码器按钮是否被按下
   */
  bool isEncoderButtonPressed();

  /**
   * @brief 重置编码器按钮状态
   */
  void resetEncoderButton();

  // ==================== 屏幕控制 ====================
  void _screenOn() override;    // 打开OLED显示
  void _screenOff() override;   // 关闭OLED显示（省电）
};

#endif

/**
 * @file SSD1306.h
 * @brief OLED显示模块 - 主界面和设置界面的绘制与动画
 */
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "sensors.h"

// OLED引脚
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_ADDR   0x3C

// 初始化OLED
void oledInit();

// 绘制主界面（传感器数据+进度条+入场动画）
void drawMonitor(SensorData data, int settingsIdx, bool editing, bool mq135Ready);

// 绘制设置界面（阈值调节）
void drawSettings(float thresholds[4], int settingsIdx, bool editing);

// 开机画面
void drawBootScreen();

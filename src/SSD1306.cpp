/**
 * @file SSD1306.cpp
 * @brief OLED显示模块 - 主界面和设置界面的绘制与动画
 */
#include "SSD1306.h"

static u8g2_t oled;

// ==================== 动画 ====================
static float animY[4] = {64, 64, 64, 64};      // 各行当前Y坐标
static float targetY[4] = {26, 38, 50, 62};    // 目标Y坐标（避开标题栏）

// 缓动动画：current向target靠近，speed越大越快
static void animate(float* current, float target, float speed) {
  *current += (target - *current) * speed;
  if (fabs(*current - target) < 0.5f) *current = target;
}

// ==================== OLED初始化 ====================
void oledInit() {
  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(&oled, U8G2_R0,
    u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
  u8g2_SetI2CAddress(&oled, OLED_ADDR << 1);
  u8g2_InitDisplay(&oled);
  u8g2_SetPowerSave(&oled, 0);
}

// ==================== 开机画面 ====================
void drawBootScreen() {
  u8g2_ClearBuffer(&oled);
  u8g2_SetFont(&oled, u8g2_font_wqy12_t_gb2312);
  u8g2_SetDrawColor(&oled, 1);
  u8g2_DrawUTF8(&oled, 10, 25, "教室环境监测系统");
  u8g2_SetFont(&oled, u8g2_font_5x7_tf);
  u8g2_DrawStr(&oled, 15, 45, "MQ135 warmup: 15s");
  u8g2_DrawStr(&oled, 20, 55, "Starting...");
  u8g2_SendBuffer(&oled);
}

// ==================== 主界面 ====================
void drawMonitor(SensorData d, int /*settingsIdx*/, bool /*editing*/, bool mq135Ready) {
  char buf[32];

  // 行入场动画
  for (int i = 0; i < 4; i++) animate(&animY[i], targetY[i], 0.15f);

  u8g2_ClearBuffer(&oled);
  u8g2_SetFont(&oled, u8g2_font_wqy12_t_gb2312);
  u8g2_SetDrawColor(&oled, 1);

  // 标题栏反色
  u8g2_DrawBox(&oled, 0, 0, 128, 14);
  u8g2_SetDrawColor(&oled, 0);
  u8g2_DrawUTF8(&oled, 2, 12, "教室环境监测");
  u8g2_SetDrawColor(&oled, 1);
  u8g2_DrawLine(&oled, 0, 15, 128, 15);

  // 光照 + 进度条
  snprintf(buf, sizeof(buf), "光照: %.0f%%", d.light);
  u8g2_DrawUTF8(&oled, 4, static_cast<int>(animY[0]), buf);
  auto bw = static_cast<int>(d.light * 0.6f); if (bw > 60) bw = 60;
  if (bw > 0) u8g2_DrawRBox(&oled, 68, static_cast<int>(animY[0]) - 9, bw, 8, 2);

  // 空气 + 进度条
  if (mq135Ready) {
    snprintf(buf, sizeof(buf), "空气: %.0fppm", d.aqi);
  } else {
    snprintf(buf, sizeof(buf), "空气: 预热中...");
  }
  u8g2_DrawUTF8(&oled, 4, static_cast<int>(animY[1]), buf);
  if (mq135Ready) {
    bw = static_cast<int>(d.aqi / 1000.0f * 60.0f); if (bw > 60) bw = 60;
    if (bw > 0) u8g2_DrawRBox(&oled, 68, static_cast<int>(animY[1]) - 9, bw, 8, 2);
  }

  // 温度
  snprintf(buf, sizeof(buf), "温度: %.1f°C", d.temp);
  u8g2_DrawUTF8(&oled, 4, static_cast<int>(animY[2]), buf);

  // 湿度
  snprintf(buf, sizeof(buf), "湿度: %.0f%%", d.humid);
  u8g2_DrawUTF8(&oled, 4, static_cast<int>(animY[3]), buf);

  u8g2_SendBuffer(&oled);
}

// ==================== 设置界面 ====================
void drawSettings(float thresholds[4], int settingsIdx, bool editing) {
  char buf[32];

  u8g2_ClearBuffer(&oled);
  u8g2_SetFont(&oled, u8g2_font_wqy12_t_gb2312);
  u8g2_SetDrawColor(&oled, 1);

  // 标题栏
  u8g2_DrawBox(&oled, 0, 0, 128, 14);
  u8g2_SetDrawColor(&oled, 0);
  u8g2_DrawUTF8(&oled, 2, 12, "阈值设置");
  u8g2_SetDrawColor(&oled, 1);
  u8g2_DrawLine(&oled, 0, 15, 128, 15);

  // 4个阈值项
  const char* labels[] = {"光照阈值", "空气阈值(PPM)", "温度阈值", "湿度阈值"};
  for (int i = 0; i < 4; i++) {
    int y = 26 + i * 12;  // 从y=26开始，避开标题栏
    snprintf(buf, sizeof(buf), "%s: %.0f", labels[i], thresholds[i]);

    if (i == settingsIdx) {
      if (editing) {
        u8g2_DrawBox(&oled, 0, y - 10, 128, 13);
        u8g2_SetDrawColor(&oled, 0);
        u8g2_DrawUTF8(&oled, 4, y, buf);
        u8g2_SetDrawColor(&oled, 1);
      } else {
        u8g2_DrawRFrame(&oled, 0, y - 10, 128, 13, 2);
        u8g2_DrawUTF8(&oled, 4, y, buf);
      }
    } else {
      u8g2_DrawUTF8(&oled, 4, y, buf);
    }
  }

  // 底部提示（小字体，不遮挡）
  u8g2_SetFont(&oled, u8g2_font_5x7_tf);
  u8g2_DrawStr(&oled, 2, 63,
    editing ? "Rotate:Adjust  Press:OK" : "Rotate:Select  Long:Back");

  u8g2_SendBuffer(&oled);
}

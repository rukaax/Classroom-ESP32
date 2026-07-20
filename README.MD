# Classroom-ESP32

教室环境监测系统，基于 ESP32-S3 + oled-ui-astra UI 框架。

## 功能

- SSD1306 OLED 屏幕（128x64, I2C）显示菜单和传感器数据
- 旋转编码器控制菜单导航（左转/右转切换，按下确认）
- 传感器数据实时采集与显示（光敏、MQ135、DHT11）
- 串口输出传感器数据（115200 波特率，每 2 秒）

## 硬件

| 组件 | 型号/规格 |
|------|----------|
| 主控 | ESP32-S3 N16R8（16MB Flash, 8MB PSRAM） |
| 显示 | SSD1306 OLED 128x64（I2C） |
| 输入 | 旋转编码器（CLK/DT/SW 三线制） |
| 光照 | 光敏电阻（ADC） |
| 空气 | MQ135 气体传感器（ADC） |
| 温湿度 | DHT11 数字传感器 |

## 引脚分配

| 功能 | GPIO |
|------|------|
| OLED SDA | 17 |
| OLED SCL | 18 |
| 编码器 CLK | 6 |
| 编码器 DT | 4 |
| 编码器 SW | 5 |
| 光敏电阻 | 7 |
| MQ135 | 15 |
| DHT11 | 16 |

引脚定义位于两处（保持一致）：
- `lib/oled-ui-astra/hal/hal_esp32.h`
- `include/sensors.h`

## 菜单结构

```
Classroom（根菜单）
  └── Sensors（传感器菜单）
        ├── Light（光照强度）
        ├── MQ135（空气质量）
        └── DHT11（温度/湿度）
```

## 构建

需要 [PlatformIO](https://platformio.org/)。

```bash
# 编译
pio run

# 上传
pio run -t upload

# 串口监视
pio device monitor
```

## 依赖库

- [U8g2](https://github.com/olikraus/u8g2) - OLED 驱动
- [DHT sensor library](https://github.com/adafruit/DHT-sensor-library) - DHT11 温湿度
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) - DHT 库依赖

## 项目结构

```
├── src/
│   ├── main.cpp          # 主程序（初始化、菜单构建、主循环）
│   └── sensors.cpp       # 传感器管理器实现
├── include/
│   └── sensors.h         # 传感器头文件与引脚定义
├── lib/
│   └── oled-ui-astra/    # OLED UI 框架
│       ├── astra/        # UI 核心（菜单、选择器、摄像机、开机动画）
│       └── hal/          # 硬件抽象层（ESP32 HAL 实现）
└── platformio.ini        # PlatformIO 项目配置
```

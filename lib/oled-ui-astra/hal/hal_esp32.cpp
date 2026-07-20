/**
 * @file hal_esp32.cpp
 * @brief ESP32硬件抽象层实现
 * @brief 将astra UI的所有硬件调用桥接到ESP32 Arduino框架
 *
 * 核心实现：
 *   - U8g2 C-API驱动SSD1306 OLED（I2C, 128x64全缓冲模式）
 *   - 旋转编码器通过硬件中断读取，按键扫描使用astra UI的状态机
 *   - 所有绘图函数通过U8g2库实现，坐标取整为像素
 */

#include "hal_esp32.h"
#include <cmath>

// ==================== 编码器全局变量（ISR共享） ====================
// 这些变量被中断服务程序(ISR)修改，必须声明为volatile
// volatile告诉编译器不要优化对这些变量的访问，每次都从内存读取

static volatile int _encPos = 0;       // 编码器位置增量（正值=右转，负值=左转）
static volatile bool _encBtn = false;  // 编码器按钮是否被按下
static bool _lastCLK = false;          // 上次CLK引脚的电平状态（用于判断转动方向）
static volatile uint32_t _isrCount = 0; // ISR触发计数器（调试用）

// ==================== 编码器中断服务程序 ====================

/**
 * @brief 编码器CLK引脚变化中断（在hal_esp32.cpp顶部定义，ISR专用）
 *
 * 工作原理（正交编码器）：
 *   编码器转动时，CLK和DT两个引脚产生相位差90度的方波
 *   - 右转：CLK先变化，DT滞后 -> DT与CLK不同相 -> _encPos++
 *   - 左转：DT先变化，CLK滞后 -> DT与CLK同相 -> _encPos--
 *
 * 使用IRAM_ATTR将函数放入ESP32的IRAM中，确保中断响应速度
 */
static void IRAM_ATTR encoderISR() {
  static uint32_t lastIsrTime = 0;
  uint32_t now = micros();
  if (now - lastIsrTime < 500) return;  // 500us消抖
  lastIsrTime = now;

  _isrCount++;
  bool dt = digitalRead(ENCODER_DT);    // 读取DT电平
  if (dt) _encPos--;                    // DT高 -> 左转
  else _encPos++;                       // DT低 -> 右转
}

/**
 * @brief 编码器按钮按下中断（下降沿触发，按钮按下时SW引脚接地）
 */
static void IRAM_ATTR encoderBtnISR() {
  _encBtn = true;  // 设置按钮按下标志
}

// ==================== 初始化 ====================

/**
 * @brief 初始化ESP32的所有硬件
 *
 * 初始化顺序：
 *   1. 串口（115200波特率）- 用于调试输出和传感器数据打印
 *   2. I2C总线（SDA=GPIO17, SCL=GPIO18）
 *   3. SSD1306 OLED（128x64, I2C地址0x3C, 全缓冲模式）
 *   4. 旋转编码器（CLK=GPIO4, DT=GPIO5, SW=GPIO6, 带中断）
 *   5. 设置屏幕分辨率为128x64
 */
void HALESP32::init() {
  // ---------- 串口初始化 ----------
  Serial.begin(115200);       // 启动串口通信，波特率115200
  delay(100);                 // 等待串口稳定

  // ---------- I2C总线初始化 ----------
  Wire.begin(OLED_SDA, OLED_SCL);  // 启动I2C，指定SDA和SCL引脚

  // ---------- SSD1306 OLED初始化 ----------
  // 使用U8g2的C语言API设置SSD1306 I2C驱动
  // 参数说明：
  //   &u8g2 - U8g2驱动实例
  //   U8G2_R0 - 屏幕旋转方向（R0=不旋转）
  //   u8x8_byte_arduino_hw_i2c - 使用Arduino硬件I2C通信
  //   u8x8_gpio_and_delay_arduino - 使用Arduino的GPIO和延时函数
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(
    &u8g2, U8G2_R0,
    u8x8_byte_arduino_hw_i2c,
    u8x8_gpio_and_delay_arduino
  );
  u8g2_SetI2CAddress(&u8g2, OLED_ADDR << 1);  // 设置I2C地址（左移1位是因为U8g2使用8位地址格式）
  u8g2_InitDisplay(&u8g2);                      // 初始化显示屏硬件
  u8g2_SetPowerSave(&u8g2, 0);                  // 关闭省电模式（打开显示）
  u8g2_ClearBuffer(&u8g2);                      // 清空显示缓冲区
  u8g2_SendBuffer(&u8g2);                       // 将空白缓冲区发送到屏幕

  // ---------- 设置屏幕分辨率 ----------
  config.screenWeight = 128;   // 屏幕宽度128像素
  config.screenHeight = 64;    // 屏幕高度64像素

  // ---------- 旋转编码器初始化 ----------
  // 配置引脚为输入模式，启用内部上拉电阻
  // 上拉电阻确保引脚在未连接时保持高电平，按钮按下时接地变为低电平
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // 读取初始CLK状态（用于中断判断转动方向）
  _lastCLK = digitalRead(ENCODER_CLK);

  // 注册硬件中断
  // CLK引脚：任何变化都触发（CHANGE），用于检测编码器转动
  // SW引脚：下降沿触发（FALLING），按钮按下时从高变低
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW), encoderBtnISR, FALLING);

  Serial.println("[ESP32] Hardware initialized.");
}

// ==================== OLED画布操作 ====================

/**
 * @brief 获取U8g2画布缓冲区指针
 * @return 缓冲区首地址，astra UI用它来操作像素数据（如模糊动画效果）
 */
void* HALESP32::_getCanvasBuffer() {
  return u8g2_GetBufferPtr(&u8g2);  // U8g2 API获取缓冲区指针
}

/**
 * @brief 获取画布高度（以tile为单位，每个tile=8像素）
 * @return 128x64屏幕返回8（64/8=8个tile高）
 */
uint8_t HALESP32::_getBufferTileHeight() {
  return u8g2_GetBufferTileHeight(&u8g2);
}

/**
 * @brief 获取画布宽度（以tile为单位）
 * @return 128x64屏幕返回16（128/8=16个tile宽）
 */
uint8_t HALESP32::_getBufferTileWidth() {
  return u8g2_GetBufferTileWidth(&u8g2);
}

/**
 * @brief 将画布缓冲区内容刷新到OLED屏幕
 * @note 这是一个I2C传输操作，将整个1KB缓冲区发送到SSD1306
 */
void HALESP32::_canvasUpdate() {
  u8g2_SendBuffer(&u8g2);
}

/**
 * @brief 清空画布（填充全黑）
 */
void HALESP32::_canvasClear() {
  u8g2_ClearBuffer(&u8g2);
}

// ==================== 字体操作 ====================

/**
 * @brief 设置当前绘图字体
 * @param _font U8g2字体数组指针，如 u8g2_font_6x12_tf
 */
void HALESP32::_setFont(const uint8_t* _font) {
  u8g2_SetFont(&u8g2, _font);
}

/**
 * @brief 计算指定文字的像素宽度（使用当前字体）
 * @param _text 要测量的文字
 * @return 文字占用的像素宽度
 */
uint8_t HALESP32::_getFontWidth(std::string& _text) {
  return u8g2_GetStrWidth(&u8g2, _text.c_str());
}

/**
 * @brief 获取当前字体的像素高度
 * @return 字体高度 = 上行高度 - 下行深度
 */
uint8_t HALESP32::_getFontHeight() {
  return u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2);
}

// ==================== 绘图颜色控制 ====================

/**
 * @brief 设置绘图颜色模式
 * @param _type 0=黑色（清除像素），1=白色（点亮像素），2=反色（XOR）
 */
void HALESP32::_setDrawType(uint8_t _type) {
  u8g2_SetDrawColor(&u8g2, _type);
}

// ==================== 基础绘图函数 ====================
// 以下函数将浮点坐标四舍五入为整数像素坐标，然后调用U8g2绘图API

/**
 * @brief 绘制单个像素
 */
void HALESP32::_drawPixel(float _x, float _y) {
  u8g2_DrawPixel(&u8g2, (int16_t)round(_x), (int16_t)round(_y));
}

/**
 * @brief 绘制UTF8文字（英文和中文都用这个）
 * @param _x 文字左下角X坐标
 * @param _y 文字左下角Y坐标
 * @param _text UTF8编码的文字字符串
 */
void HALESP32::_drawEnglish(float _x, float _y, const std::string& _text) {
  u8g2_DrawUTF8(&u8g2, (int16_t)round(_x), (int16_t)round(_y), _text.c_str());
}

/**
 * @brief 绘制中文文字（U8g2内部处理UTF8编码）
 */
void HALESP32::_drawChinese(float _x, float _y, const std::string& _text) {
  u8g2_DrawUTF8(&u8g2, (int16_t)round(_x), (int16_t)round(_y), _text.c_str());
}

/**
 * @brief 绘制垂直虚线（每隔2像素画2像素，跳过2像素）
 */
void HALESP32::_drawVDottedLine(float _x, float _y, float _h) {
  for (int16_t i = 0; i < (int16_t)round(_h); i++) {
    if (i % 4 < 2) continue;  // 跳过前2个像素，画后2个像素，循环
    u8g2_DrawPixel(&u8g2, (int16_t)round(_x), (int16_t)round(_y) + i);
  }
}

/**
 * @brief 绘制水平虚线
 */
void HALESP32::_drawHDottedLine(float _x, float _y, float _l) {
  for (int16_t i = 0; i < (int16_t)round(_l); i++) {
    if (i % 4 < 2) continue;
    u8g2_DrawPixel(&u8g2, (int16_t)round(_x) + i, (int16_t)round(_y));
  }
}

/**
 * @brief 绘制垂直实线
 */
void HALESP32::_drawVLine(float _x, float _y, float _h) {
  u8g2_DrawVLine(&u8g2, (int16_t)round(_x), (int16_t)round(_y), (int16_t)round(_h));
}

/**
 * @brief 绘制水平实线
 */
void HALESP32::_drawHLine(float _x, float _y, float _l) {
  u8g2_DrawHLine(&u8g2, (int16_t)round(_x), (int16_t)round(_y), (int16_t)round(_l));
}

/**
 * @brief 绘制XBM位图（astra UI用它显示菜单图标）
 * @param _x 位图左上角X坐标
 * @param _y 位图左上角Y坐标
 * @param _w 位图宽度（像素）
 * @param _h 位图高度（像素）
 * @param _bitMap XBM格式的位图数据数组
 */
void HALESP32::_drawBMP(float _x, float _y, float _w, float _h, const uint8_t* _bitMap) {
  u8g2_DrawXBM(&u8g2, (int16_t)round(_x), (int16_t)round(_y),
               (int16_t)round(_w), (int16_t)round(_h), _bitMap);
}

/**
 * @brief 绘制填充矩形
 */
void HALESP32::_drawBox(float _x, float _y, float _w, float _h) {
  u8g2_DrawBox(&u8g2, (int16_t)round(_x), (int16_t)round(_y),
               (int16_t)round(_w), (int16_t)round(_h));
}

/**
 * @brief 绘制填充圆角矩形（astra UI选择框用这个）
 */
void HALESP32::_drawRBox(float _x, float _y, float _w, float _h, float _r) {
  u8g2_DrawRBox(&u8g2, (int16_t)round(_x), (int16_t)round(_y),
                (int16_t)round(_w), (int16_t)round(_h), (int16_t)round(_r));
}

/**
 * @brief 绘制空心矩形边框
 */
void HALESP32::_drawFrame(float _x, float _y, float _w, float _h) {
  u8g2_DrawFrame(&u8g2, (int16_t)round(_x), (int16_t)round(_y),
                 (int16_t)round(_w), (int16_t)round(_h));
}

/**
 * @brief 绘制空心圆角矩形边框（磁贴选择框用这个）
 */
void HALESP32::_drawRFrame(float _x, float _y, float _w, float _h, float _r) {
  u8g2_DrawRFrame(&u8g2, (int16_t)round(_x), (int16_t)round(_y),
                  (int16_t)round(_w), (int16_t)round(_h), (int16_t)round(_r));
}

// ==================== 系统时间函数 ====================

/**
 * @brief 延时指定毫秒（阻塞当前线程）
 */
void HALESP32::_delay(unsigned long _mill) {
  ::delay(_mill);
}

/**
 * @brief 获取开机以来的毫秒数
 * @note ESP32的millis()精度约1ms，使用FreeRTOS节拍器实现
 */
unsigned long HALESP32::_millis() {
  return ::millis();
}

/**
 * @brief 获取系统节拍（与millis相同）
 */
unsigned long HALESP32::_getTick() {
  return ::millis();
}

/**
 * @brief 获取随机种子
 * @brief 使用光敏电阻ADC读数与微秒计时器异或，产生伪随机种子
 * @note 由于光敏电阻读数受环境光照影响，每次上电的种子不同
 */
unsigned long HALESP32::_getRandomSeed() {
  return analogRead(PIN_LIGHT) ^ micros();
}

uint32_t HALESP32::getISRCount() {
  return _isrCount;
}

int HALESP32::getEncoderPos() {
  return _encPos;
}

// ==================== 按键接口 ====================

/**
 * @brief 检查指定按键是否处于"按下"状态
 * @param _keyIndex KEY_0=左转，KEY_1=右转或按钮按下
 * @return true=按下，false=未按下
 *
 * 注意：这个函数只返回瞬时状态，真正的消抖和长短按判断由_keyScan()状态机处理
 */
bool HALESP32::_getKey(key::KEY_INDEX _keyIndex) {
  if (_keyIndex == key::KEY_0) {
    return _encPos < 0;   // 编码器左转 -> KEY_0按下
  }
  if (_keyIndex == key::KEY_1) {
    return _encPos > 0 || digitalRead(ENCODER_SW) == LOW;  // 右转或按钮按下 -> KEY_1按下
  }
  return false;
}

/**
 * @brief 检查是否有任何待处理的按键事件
 *
 * 重写基类实现：基类的 _getAnyKey() 调用 _getKey() 检查硬件瞬时状态，
 * 但编码器的 _encPos 已经被 _keyScan() 消费清零了。
 * 这里改为检查 key[] 数组，它保存了 _keyScan() 的扫描结果。
 */
bool HALESP32::_getAnyKey() {
  for (int i = 0; i < key::KEY_NUM; i++) {
    if (key[i] != key::RELEASE) return true;
  }
  return false;
}

// ==================== 编码器按键扫描状态机 ====================
/**
 * @brief 重写astra UI的按键扫描函数，适配旋转编码器
 *
 * 与原始实现的区别：
 *   - 原始实现假设KEY_0和KEY_1是独立的物理按钮（持续按住）
 *   - 编码器的"按键"是瞬时的（转一下就没了），需要用增量值判断
 *
 * 状态机流程：
 *   CHECKING -> 检测到输入 -> KEY_0_CONFIRM 或 KEY_1_CONFIRM
 *   KEY_x_CONFIRM -> 持续按住超过200次扫描(约1秒) -> PRESS（长按）
 *   KEY_x_CONFIRM -> 松开 -> CLICK（短按）
 *   RELEASED -> 松开后 -> CHECKING（回到等待状态）
 *
 * 扫描周期：5ms（每5ms检查一次按键状态）
 */
void HALESP32::_keyScan() {
  static unsigned long lastScan = 0;
  unsigned long now = millis();
  if (now - lastScan < 5) return;  // 每5ms扫描一次
  lastScan = now;

  static uint8_t timeCnt = 0;                    // 长按计时器
  static bool lock = false;                       // 锁定标志（防止松开时重复触发）
  static key::KEY_FILTER keyFilter = key::CHECKING; // 当前状态
  static bool wasBtnPress = false;                // 记录是否是按钮触发

  // 读取当前输入状态
  bool leftTurn = _encPos < 0;                    // 编码器是否左转
  bool rightTurn = _encPos > 0;                   // 编码器是否右转
  bool btnPressed = digitalRead(ENCODER_SW) == LOW; // 按钮是否按下
  bool anyInput = leftTurn || rightTurn || btnPressed; // 是否有任何输入

  switch (keyFilter) {
    case key::CHECKING:
      // 等待状态：检测是否有输入
      if (anyInput) {
        if (leftTurn) { keyFilter = key::KEY_0_CONFIRM; wasBtnPress = false; lock = true; }
        else if (rightTurn || btnPressed) { keyFilter = key::KEY_1_CONFIRM; wasBtnPress = btnPressed; lock = true; }
        _encPos = 0;              // 清除增量，下次扫描不再重复触发
      }
      timeCnt = 0;
      break;

    case key::KEY_0_CONFIRM:
    case key::KEY_1_CONFIRM:
      // 确认状态：等待松开或达到长按阈值
      if (anyInput) {
        if (!lock) lock = true;     // 标记已锁定（防止松开时误触发）
        timeCnt++;                  // 长按计时

        if (timeCnt > 200) {
          // 长按超过200次扫描（约1秒）-> 判定为长按(PRESS)
          if (keyFilter == key::KEY_0_CONFIRM) {
            key[key::KEY_0] = key::PRESS;
            key[key::KEY_1] = key::RELEASE;
            Serial.println("[Encoder] LEFT long press");
          } else {
            key[key::KEY_1] = key::PRESS;
            key[key::KEY_0] = key::RELEASE;
            Serial.println(wasBtnPress ? "[Encoder] Button long press" : "[Encoder] RIGHT long press");
          }
          timeCnt = 0;
          lock = false;
          _encPos = 0;              // 清除编码器增量
          keyFilter = key::RELEASED;
        }
      } else {
        // 松开了
        if (lock) {
          // 之前有锁定 -> 判定为短按(CLICK)
          if (keyFilter == key::KEY_0_CONFIRM) {
            key[key::KEY_0] = key::CLICK;
            key[key::KEY_1] = key::RELEASE;
            Serial.println("[Encoder] LEFT turn");
          } else {
            key[key::KEY_1] = key::CLICK;
            key[key::KEY_0] = key::RELEASE;
            Serial.println(wasBtnPress ? "[Encoder] Button click" : "[Encoder] RIGHT turn");
          }
          _encPos = 0;              // 清除编码器增量
          keyFilter = key::RELEASED;
        } else {
          // 没有锁定（抖动或误触发）-> 回到等待状态
          keyFilter = key::CHECKING;
          key[key::KEY_0] = key[key::KEY_1] = key::RELEASE;
        }
      }
      break;

    case key::RELEASED:
      // 已释放状态：等待所有输入松开后回到等待状态
      if (!anyInput) {
        keyFilter = key::CHECKING;
        _encPos = 0;                // 确保编码器增量清零
      }
      break;

    default: break;
  }
}

// ==================== 屏幕控制 ====================

/**
 * @brief 打开OLED显示
 */
void HALESP32::_screenOn() {
  u8g2_SetPowerSave(&u8g2, 0);  // 0=正常模式（显示开启）
}

/**
 * @brief 关闭OLED显示（省电模式）
 */
void HALESP32::_screenOff() {
  u8g2_SetPowerSave(&u8g2, 1);  // 1=省电模式（显示关闭）
}

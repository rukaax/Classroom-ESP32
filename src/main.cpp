/**
 * @file main.cpp
 * @brief Classroom Environment Monitor
 * @brief ESP32-S3 + SSD1306 OLED + Rotary Encoder + Sensors
 *
 * Simple menu system:
 *   Rotate encoder = navigate menu
 *   Short press    = enter detail view / back to menu
 *   Long press     = back to menu from detail view
 *
 * Hardware:
 *   OLED:   SDA=GPIO17, SCL=GPIO18, 128x64 SSD1306
 *   Encoder: CLK=GPIO4, DT=GPIO5, SW=GPIO6
 *   Light:  GPIO7 (ADC)
 *   MQ135:  GPIO15 (ADC)
 *   DHT11:  GPIO16
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "sensors.h"

// ==================== Pin Definitions ====================
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_ADDR   0x3C

#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6

// ==================== Global Objects ====================
u8g2_t u8g2;
SensorManager sensors(2000);

// ==================== Encoder Variables ====================
volatile int encPos = 0;
static uint32_t lastIsrTime = 0;

void IRAM_ATTR encoderISR() {
  uint32_t now = micros();
  if (now - lastIsrTime < 500) return;
  lastIsrTime = now;
  bool dt = digitalRead(ENCODER_DT);
  if (dt) encPos--;
  else encPos++;
}

// ==================== Menu Definition ====================
enum MenuState { MENU_ROOT, MENU_DETAIL };

const char* menuItems[] = { "Light", "MQ135", "Temp", "Humidity" };
const int MENU_COUNT = 4;
int menuIndex = 0;
MenuState state = MENU_ROOT;

// ==================== Button Handling ====================
unsigned long btnPressStart = 0;
bool btnWasPressed = false;
bool btnShortPress = false;
bool btnLongPress = false;

void updateButton() {
  bool pressed = digitalRead(ENCODER_SW) == LOW;
  btnShortPress = false;
  btnLongPress = false;

  if (pressed && !btnWasPressed) {
    btnPressStart = millis();
    btnWasPressed = true;
  }
  if (!pressed && btnWasPressed) {
    unsigned long duration = millis() - btnPressStart;
    btnWasPressed = false;
    if (duration < 500) btnShortPress = true;
    else btnLongPress = true;
  }
}

// ==================== Display Functions ====================
void drawMenu() {
  u8g2_ClearBuffer(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_6x12_tf);

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 12 + i * 14;
    if (i == menuIndex) {
      u8g2_DrawRBox(&u8g2, 0, y - 10, 128, 13, 2);
      u8g2_SetDrawColor(&u8g2, 0);
      u8g2_DrawStr(&u8g2, 4, y, menuItems[i]);
      u8g2_SetDrawColor(&u8g2, 1);
    } else {
      u8g2_DrawStr(&u8g2, 4, y, menuItems[i]);
    }
  }

  u8g2_SendBuffer(&u8g2);
}

void drawDetail() {
  SensorData d = sensors.getData();
  u8g2_ClearBuffer(&u8g2);

  const char* title = menuItems[menuIndex];
  char valueStr[32];

  switch (menuIndex) {
    case 0: snprintf(valueStr, sizeof(valueStr), "%.1f%%", d.lightLevel); break;
    case 1: snprintf(valueStr, sizeof(valueStr), "%.1f%%", d.mq135Value); break;
    case 2: snprintf(valueStr, sizeof(valueStr), "%.1f C", d.temperature); break;
    case 3: snprintf(valueStr, sizeof(valueStr), "%.1f%%", d.humidity); break;
    default: snprintf(valueStr, sizeof(valueStr), "---"); break;
  }

  // Title
  u8g2_SetFont(&u8g2, u8g2_font_6x12_tf);
  u8g2_DrawStr(&u8g2, 4, 12, title);

  // Value - large font
  u8g2_SetFont(&u8g2, u8g2_font_logisoso32_tf);
  int valWidth = u8g2_GetStrWidth(&u8g2, valueStr);
  int valX = (128 - valWidth) / 2;
  u8g2_DrawStr(&u8g2, valX, 56, valueStr);

  // Back hint
  u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
  u8g2_DrawStr(&u8g2, 0, 64, "< Back");

  u8g2_SendBuffer(&u8g2);
}

// ==================== Serial Output ====================
unsigned long lastSerialPrint = 0;
const unsigned long SERIAL_INTERVAL = 2000;

void printSensorData() {
  SensorData d = sensors.getData();
  Serial.println("===== Sensor Data =====");
  Serial.print("Light:  "); Serial.println(sensors.getLightString());
  Serial.print("MQ135:  "); Serial.println(sensors.getMQ135String());
  Serial.print("Temp:   "); Serial.println(sensors.getTempString());
  Serial.print("Humid:  "); Serial.println(sensors.getHumidityString());
  Serial.println("=======================");
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);

  // I2C + OLED - use C API (proven working from old code)
  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(
    &u8g2, U8G2_R0,
    u8x8_byte_arduino_hw_i2c,
    u8x8_gpio_and_delay_arduino
  );
  u8g2_SetI2CAddress(&u8g2, OLED_ADDR << 1);
  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
  u8g2_ClearBuffer(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_6x12_tf);
  u8g2_DrawStr(&u8g2, 20, 36, "Classroom ESP32");
  u8g2_SendBuffer(&u8g2);
  delay(1000);

  // Encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, RISING);

  // Sensors
  sensors.begin();

  Serial.println("[Main] System ready.");
}

// ==================== Loop ====================
void loop() {
  sensors.update();
  updateButton();

  // Read encoder delta
  int delta = 0;
  noInterrupts();
  if (encPos != 0) {
    delta = encPos;
    encPos = 0;
  }
  interrupts();

  if (state == MENU_ROOT) {
    // Navigate menu
    if (delta > 0 && menuIndex < MENU_COUNT - 1) menuIndex++;
    if (delta < 0 && menuIndex > 0) menuIndex--;

    // Enter detail on short press
    if (btnShortPress) state = MENU_DETAIL;

    drawMenu();
  } else {
    // Detail view - short press or long press goes back
    if (btnShortPress || btnLongPress) state = MENU_ROOT;

    drawDetail();
  }

  // Serial output
  unsigned long now = millis();
  if (now - lastSerialPrint >= SERIAL_INTERVAL) {
    lastSerialPrint = now;
    printSensorData();
  }

  delay(10);
}

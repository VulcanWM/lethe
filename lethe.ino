#include <Wire.h>
#include <U8g2lib.h>
#include <7Semi_BMI270.h>
#include <BackgroundAudio.h>
#include <ESP32I2SAudio.h>

// buttons
#define BUTTON_POWER D3
#define BUTTON_HOME D4

// i2c (for oled and imu)
#define SDA_PIN D5
#define SCL_PIN D6

// imu interrupt
#define IMU_INT1 D11

// i2s (for amplifier)
#define I2S_DOUT D12
#define I2S_BCLK D13
#define I2S_LRCLK D14

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

BMI270_7Semi imu;

ESP32I2SAudio audio(
  I2S_BCLK,
  I2S_LRCLK,
  I2S_DOUT
);

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  BMI270_7Semi::Config cfg;

  cfg.bus = BMI270_7Semi::Bus::I2C;
  cfg.addr = 0x69;
  cfg.sda = SDA_PIN;
  cfg.scl = SCL_PIN;
  cfg.i2cHz = 400000;

  if (!imu.begin(cfg)){
    Serial.println("BMI270 failed");
  }

  imu.setAccelConfig(
    BMI2_ACC_ODR_100HZ,
    BMI2_ACC_RANGE_2G,
    BMI2_ACC_NORMAL_AVG4,
    BMI2_PERF_OPT_MODE
  );

  display.begin();

  pinMode(BUTTON_POWER, INPUT_PULLUP);
  pinMode(BUTTON_HOME, INPUT_PULLUP);
}

void loop() {
  bool powerPressed = digitalRead(BUTTON_POWER) == LOW;
  bool homePressed = digitalRead(BUTTON_HOME) == LOW;

  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 12, "hello lethe");
  
  display.setCursor(0, 32);
  display.print("power: ");
  display.print(powerPressed ? "pressed" : "released");

  display.setCursor(0, 48);
  display.print("home: ");
  display.print(homePressed ? "pressed" : "released");

  display.sendBuffer();

  float ax, ay, az;
  if (imu.readAccel(ax, ay, az)){
    Serial.print("x: ");
    Serial.println(ax);

    Serial.print("y: ");
    Serial.println(ay);

    Serial.print("z: ");
    Serial.println(az);
  }
  
  delay(20);
}

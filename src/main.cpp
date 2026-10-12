// ESP32 Dev Kit + MPU6050 -> "roll,pitch" lines over USB serial @115200
// Wiring: VCC->3V3  GND->GND  SDA->GPIO21  SCL->GPIO22
// Prints "ERR,<reason>" once per second if the sensor is not responding.
#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22

uint8_t addr = 0x68;
bool mpuOk = false, firstSample = true;
float roll = 0, pitch = 0;
unsigned long lastMicros = 0, lastSend = 0, lastErr = 0;

bool probe(uint8_t a) {
  Wire.beginTransmission(a);
  Wire.write(0x75);                          // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, (uint8_t)1) != 1) return false;
  uint8_t id = Wire.read();
  return id == 0x68 || id == 0x69 || id == 0x70 || id == 0x71 || id == 0x72 || id == 0x98;
}

bool initMPU() {
  if (probe(0x68)) addr = 0x68;
  else if (probe(0x69)) addr = 0x69;
  else return false;
  Wire.beginTransmission(addr);
  Wire.write(0x6B); Wire.write(0x00);        // wake up
  if (Wire.endTransmission(true) != 0) return false;
  delay(50);
  lastMicros = micros();
  firstSample = true;
  return true;
}

int16_t rd16() { int16_t h = Wire.read(); return (h << 8) | Wire.read(); }

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  delay(200);
  mpuOk = initMPU();
}

void loop() {
  if (!mpuOk) {
    if (millis() - lastErr >= 1000) {
      lastErr = millis();
      Serial.println("ERR,MPU6050 not found - check SDA=GPIO21 SCL=GPIO22 3V3 GND");
      mpuOk = initMPU();
    }
    return;
  }

  Wire.beginTransmission(addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  if (Wire.requestFrom(addr, (uint8_t)14) != 14) {
    mpuOk = false;
    Serial.println("ERR,MPU6050 read failed - loose wire?");
    return;
  }

  float ax = rd16(), ay = rd16(), az = rd16();
  rd16();                                    // temperature
  float gx = rd16(), gy = rd16();

  unsigned long now = micros();
  float dt = (now - lastMicros) / 1e6f;
  lastMicros = now;

  float accRoll  = atan2f(ay, az) * RAD_TO_DEG;
  float accPitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;

  if (firstSample) { roll = accRoll; pitch = accPitch; firstSample = false; }
  else {
    roll  = 0.96f * (roll  + gx / 131.0f * dt) + 0.04f * accRoll;
    pitch = 0.96f * (pitch + gy / 131.0f * dt) + 0.04f * accPitch;
  }

  if (millis() - lastSend >= 20) {           // 50 Hz
    lastSend = millis();
    Serial.print(roll, 1);
    Serial.print(',');
    Serial.println(pitch, 1);
  }
}

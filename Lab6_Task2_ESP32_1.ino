#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define I2C_SDA         21    // D5 in Proteus
#define I2C_SCL         22    // D6 in Proteus
#define UART2_RX_PIN    16    // D0 — receive from ESP32-2
#define UART2_TX_PIN    17    // D1 — transmit to   ESP32-2

#define FALL_IMPACT_G       15.0f   // m/s² spike = impact  (~1.5g)
#define FALL_FREEFALL_G     3.0f    // m/s² near-zero = freefall
#define FREEFALL_CONFIRM_MS 300     // ms after freefall before impact check

Adafruit_MPU6050 mpu;

bool          in_freefall       = false;
unsigned long freefall_start_ms = 0;
bool          fall_detected     = false;
unsigned long fall_clear_ms     = 0;    // auto-clear fall flag after 3s

float accel_magnitude(sensors_event_t &a) {
  return sqrt(a.acceleration.x * a.acceleration.x +
              a.acceleration.y * a.acceleration.y +
              a.acceleration.z * a.acceleration.z);
}

void detect_fall(float mag) {
  unsigned long now = millis();

  if (fall_detected && (now - fall_clear_ms > 3000)) {
    fall_detected = false;
    Serial.println("[Fall] Cleared (3s latch expired)");
  }

  if (!in_freefall && mag < FALL_FREEFALL_G) {
    in_freefall       = true;
    freefall_start_ms = now;
    Serial.println("[Fall] Free-fall detected...");
    return;
  }

  if (in_freefall) {
    if (mag > FALL_IMPACT_G) {
      if ((now - freefall_start_ms) < FREEFALL_CONFIRM_MS) {
        fall_detected   = true;
        fall_clear_ms   = now;
        Serial.print("[Fall] *** FALL CONFIRMED *** magnitude=");
        Serial.print(mag, 2);
        Serial.println(" m/s²");
      }
      in_freefall = false;
    }
    else if ((now - freefall_start_ms) > FREEFALL_CONFIRM_MS) {
      in_freefall = false;
    }
  }
}

void setup() {
  Serial.begin(115200);                                       // USB debug
  Serial2.begin(9600, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN); // to ESP32-2

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin()) {
    Serial.println("[MPU6050] NOT FOUND — check D5(SDA)/D6(SCL) wiring!");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  Serial.println("[MPU6050] Connected ");
  Serial.println("TX → ESP32-2 via Serial2 (GPIO17)");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float mag = accel_magnitude(a);
  detect_fall(mag);

  Serial2.print(a.acceleration.x, 2);
  Serial2.print(",");
  Serial2.print(a.acceleration.y, 2);
  Serial2.print(",");
  Serial2.print(a.acceleration.z, 2);
  Serial2.print(",");
  Serial2.print(temp.temperature, 1);
  Serial2.print(",");
  Serial2.println(fall_detected ? "1" : "0");

  Serial.print("[TX] X:");
  Serial.print(a.acceleration.x, 2);
  Serial.print(" Y:");
  Serial.print(a.acceleration.y, 2);
  Serial.print(" Z:");
  Serial.print(a.acceleration.z, 2);
  Serial.print(" T:");
  Serial.print(temp.temperature, 1);
  Serial.print("°C |");
  Serial.println(fall_detected ? " FALL " : "  Normal ");

  while (Serial2.available()) {
    String cmd = Serial2.readStringUntil('\n');
    cmd.trim();
    Serial.print("[RX from ESP32-2] ");
    Serial.println(cmd);
  }

  delay(500);
}
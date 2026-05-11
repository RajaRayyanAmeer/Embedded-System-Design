#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define BUZZER_PIN      32    // D9 in Proteus  ← FIXED (was 5)
#define LED_PIN         15    // Status LED (optional)
#define I2C_SDA         21    // D5
#define I2C_SCL         22    // D6
#define UART2_RX_PIN    16    // D0 — receive from ESP32-1
#define UART2_TX_PIN    17    // D1 — transmit to   ESP32-1

#define LCD_ADDRESS     0x27
#define OLED_ADDRESS    0x3C

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
Adafruit_SSD1306  oled(128, 64, &Wire, -1);
bool              oled_ok = false;

float         accel_x     = 0;
float         accel_y     = 0;
float         accel_z     = 0;
float         temperature = 0;
bool          fall_flag   = false;

unsigned long last_data_ms = 0;   // Timestamp of last valid packet

bool          auto_mode   = true;   // AUTO: buzzer follows sensor

String        uart_buf    = "";   // UART2 receive buffer
String        usb_buf     = "";   // USB Serial command buffer

unsigned long last_beep_ms = 0;
int           beep_count   = 0;

void setup() {
  Serial.begin(115200);                                           // USB debug/commands
  Serial2.begin(9600, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);  // from ESP32-1

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN,    OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN,    LOW);

  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  SmartCare v2  ");
  lcd.setCursor(0, 1);
  lcd.print(" Fall Detector  ");
  delay(1500);
  lcd.clear();

  oled_ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (!oled_ok) {
    Serial.println("[OLED] Not found at 0x3C — check wiring");
  } else {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 10);
    oled.println("SmartCare");
    oled.setTextSize(1);
    oled.setCursor(10, 40);
    oled.println("Fall Detector v2");
    oled.setCursor(20, 52);
    oled.println("Initializing...");
    oled.display();
    delay(1500);
    oled.clearDisplay();
    oled.display();
  }

  Serial.println("RX ← ESP32-1 via Serial2 (GPIO16)");
  Serial.println("Buzzer on GPIO32 (D9)");
  Serial.println("USB Commands: MODE AUTO | MODE MANUAL");
}

void process_uart() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      uart_buf.trim();

      if (uart_buf.length() > 3) {
        int c1 = uart_buf.indexOf(',');
        int c2 = uart_buf.indexOf(',', c1 + 1);
        int c3 = uart_buf.indexOf(',', c2 + 1);
        int c4 = uart_buf.indexOf(',', c3 + 1);

        if (c1 > 0 && c4 > c3) {
          accel_x     = uart_buf.substring(0, c1).toFloat();
          accel_y     = uart_buf.substring(c1 + 1, c2).toFloat();
          accel_z     = uart_buf.substring(c2 + 1, c3).toFloat();
          temperature = uart_buf.substring(c3 + 1, c4).toFloat();
          fall_flag   = (uart_buf.substring(c4 + 1).toInt() == 1);
          last_data_ms = millis();

          Serial.print("[RX] X:");
          Serial.print(accel_x, 2);
          Serial.print(" Y:");
          Serial.print(accel_y, 2);
          Serial.print(" Z:");
          Serial.print(accel_z, 2);
          Serial.print(" T:");
          Serial.print(temperature, 1);
          Serial.print("°C |");
          Serial.println(fall_flag ? " FALL" : " Normal ");
        }
      }
      uart_buf = "";
    } else if (c != '\r') {
      if (uart_buf.length() < 64) uart_buf += c;
    }
  }
}

void process_usb() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      usb_buf.trim();
      usb_buf.toUpperCase();

      if (usb_buf == "MODE AUTO") {
        auto_mode = true;
        Serial.println("[CMD] AUTO mode — buzzer follows sensor");
        Serial2.println("MODE AUTO");

      } else if (usb_buf == "MODE MANUAL") {
        auto_mode = false;
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN,    LOW);
        beep_count = 0;
        Serial.println("[CMD] MANUAL mode — buzzer silenced");
        Serial2.println("MODE MANUAL");

      } else if (usb_buf.length() > 0) {
        Serial.println("[CMD] Unknown. Try: MODE AUTO | MODE MANUAL");
      }
      usb_buf = "";
    } else if (c != '\r') {
      usb_buf += c;
    }
  }
}

void update_lcd() {
  bool data_ok = (millis() - last_data_ms < 3000);

  lcd.setCursor(0, 0);
  if (data_ok) {
    char row0[17];
    snprintf(row0, sizeof(row0), "T:%.1fC %s  ",
             temperature, auto_mode ? "AUTO  " : "MANUAL");
    lcd.print(row0);
  } else {
    lcd.print(auto_mode ? "AUTO  No Data   " : "MANUAL No Data  ");
  }

  lcd.setCursor(0, 1);
  if (!data_ok) {
    lcd.print("Waiting...      ");
  } else if (fall_flag) {
    lcd.print("!! FALL DETECT !!");
  } else {
    char row1[17];
    snprintf(row1, sizeof(row1), "X:%.1f Y:%.1f   ", accel_x, accel_y);
    lcd.print(row1);
  }
}

void update_oled() {
  if (!oled_ok) return;

  bool data_ok = (millis() - last_data_ms < 3000);

  oled.clearDisplay();

  oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setCursor(8, 2);
  oled.print("FALL DETECTION SYSTEM");
  oled.setTextColor(SSD1306_WHITE);

  if (data_ok) {
    oled.setTextSize(1);
    oled.setCursor(0, 15);
    oled.print("X:");
    oled.print(accel_x, 2);
    oled.print("  Y:");
    oled.print(accel_y, 2);

    oled.setCursor(0, 26);
    oled.print("Z:");
    oled.print(accel_z, 2);
    oled.print("  T:");
    oled.print(temperature, 1);
    oled.print("C");

    oled.setCursor(0, 40);
    oled.print("Mode: ");
    oled.print(auto_mode ? "AUTO" : "MANUAL");

    float mag = sqrt(accel_x * accel_x + accel_y * accel_y + accel_z * accel_z);
    oled.setCursor(70, 40);
    oled.print("|g|:");
    oled.print(mag, 1);

    if (fall_flag) {
      oled.fillRect(0, 52, 128, 12, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setTextSize(1);
      oled.setCursor(12, 55);
      oled.print("!! FALL DETECTED !!");
      oled.setTextColor(SSD1306_WHITE);
    } else {
      oled.drawRect(0, 52, 128, 12, SSD1306_WHITE);
      oled.setCursor(38, 55);
      oled.print("NORMAL");
    }

  } else {
    oled.setTextSize(1);
    oled.setCursor(15, 28);
    oled.print("Waiting for data");
    oled.setCursor(20, 42);
    oled.print("from ESP32-1...");
  }

  oled.display();
}

void handle_buzzer() {
  bool should_alert = fall_flag && auto_mode;

  if (should_alert) {
    if (beep_count < 6 && millis() - last_beep_ms > 200) {
      bool on = (beep_count % 2 == 0);
      digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
      digitalWrite(LED_PIN,    on ? HIGH : LOW);
      beep_count++;
      last_beep_ms = millis();
    }
    if (beep_count >= 6) beep_count = 0;   // restart burst

  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN,    LOW);
    beep_count = 0;
  }
}

void check_timeout() {
  if (accel_x != 0 && millis() - last_data_ms > 3000) {
    Serial.println("[WARN] No data from ESP32-1 for 3s");
    accel_x = accel_y = accel_z = temperature = 0;
    fall_flag = false;
  }
}

unsigned long last_display_ms = 0;

void loop() {
  process_uart();
  process_usb();
  check_timeout();

  if (millis() - last_display_ms >= 300) {
    last_display_ms = millis();
    update_lcd();
    update_oled();
  }

  handle_buzzer();
}
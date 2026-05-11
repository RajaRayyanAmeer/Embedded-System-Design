#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define BUZZER_PIN      32    // D9 in Proteus 
#define LED_PIN         15    // Optional status LED
#define I2C_SDA         21    // D5
#define I2C_SCL         22    // D6
#define UART2_RX_PIN    16    // D0 — receive from ESP32-1
#define UART2_TX_PIN    17    // D1 — transmit to   ESP32-1

#define LCD_ADDRESS     0x27
#define OLED_ADDRESS    0x3C

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
Adafruit_SSD1306  oled(128, 64, &Wire, -1);

bool          alarm_enabled     = true;
int           heart_rate        = 0;
int           spo2_level        = 0;
bool          critical_state    = false;
bool          oled_ok           = false;

String        serial_buffer     = "";   // UART2 receive buffer
unsigned long last_data_ms      = 0;    // Timestamp of last valid packet
unsigned long last_alarm_ms     = 0;    // Buzzer toggle timer
bool          buzzer_state      = false;

String        usb_buffer        = "";

void lcd_clear_row(int row) {
  lcd.setCursor(0, row);
  lcd.print("                ");
}

void show_splash() {
  lcd.setCursor(0, 0);
  lcd.print("  SmartCare v2  ");
  lcd.setCursor(0, 1);
  lcd.print(" Waiting data.. ");
  delay(2000);
  lcd.clear();

  if (oled_ok) {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(5, 10);
    oled.println("SmartCare");
    oled.setTextSize(1);
    oled.setCursor(15, 40);
    oled.println("Patient Monitor");
    oled.setCursor(20, 52);
    oled.println("Initializing...");
    oled.display();
    delay(2000);
    oled.clearDisplay();
    oled.display();
  }
}

void setup() {
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN,    OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN,    LOW);

  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();

  oled_ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (!oled_ok) {
    Serial.println("[OLED] Not found at 0x3C, check wiring");
  } else {
    Serial.println("[OLED] Connected");
  }

  show_splash();

  Serial.println("RX ← ESP32-1 via Serial2 (GPIO16)");
  Serial.println("Buzzer on GPIO32 (D9)");
  Serial.println("USB Commands: ALARM OFF | ALARM ON");
}

void process_uart_data() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      serial_buffer.trim();

      if (serial_buffer.length() > 0) {
        int c1 = serial_buffer.indexOf(',');
        int c2 = serial_buffer.indexOf(',', c1 + 1);

        if (c1 > 0 && c2 > c1) {
          int new_hr   = serial_buffer.substring(0, c1).toInt();
          int new_spo2 = serial_buffer.substring(c1 + 1, c2).toInt();
          int new_crit = serial_buffer.substring(c2 + 1).toInt();

          if (new_hr >= 0 && new_hr <= 250 &&
              new_spo2 >= 0 && new_spo2 <= 100) {
            heart_rate    = new_hr;
            spo2_level    = new_spo2;
            critical_state = (new_crit == 1);
            last_data_ms   = millis();

            Serial.print("[RX] HR: ");
            Serial.print(heart_rate);
            Serial.print(" BPM | SpO2: ");
            Serial.print(spo2_level);
            Serial.print("% | ");
            Serial.println(critical_state ? " CRITICAL " : " NORMAL ");
          }
        }
      }
      serial_buffer = "";
    } else if (c != '\r') {
      if (serial_buffer.length() < 32) 
        serial_buffer += c;
    }
  }
}

void process_usb_commands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      usb_buffer.trim();
      usb_buffer.toUpperCase();

      if (usb_buffer == "ALARM OFF") {
        alarm_enabled = false;
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN,    LOW);
        buzzer_state = false;
        Serial.println("[CMD] Alarm silenced");
        Serial2.println("ALARM OFF");

      } else if (usb_buffer == "ALARM ON") {
        alarm_enabled = true;
        Serial.println("[CMD] Alarm re-enabled");
        Serial2.println("ALARM ON");

      } else if (usb_buffer.length() > 0) {
        Serial.println("[CMD] Unknown. Try: ALARM OFF | ALARM ON");
      }
      usb_buffer = "";
    } else if (c != '\r') {
      usb_buffer += c;
    }
  }
}

void update_lcd() {
  bool data_ok = (millis() - last_data_ms < 5000);

  lcd.setCursor(0, 0);
  if (data_ok && heart_rate > 0) {
    char row0[17];
    snprintf(row0, sizeof(row0), "HR:%-4d BPM     ", heart_rate);
    lcd.print(row0);
  } else {
    lcd.print("HR: -- BPM      ");
  }

  lcd.setCursor(0, 1);
  if (data_ok && spo2_level > 0) {
    char row1[17];
    const char* status = critical_state ? "CRIT!" : "OK   ";
    snprintf(row1, sizeof(row1), "SpO2:%-3d%% %s", spo2_level, status);
    lcd.print(row1);
  } else {
    lcd.print("SpO2: --%        ");
  }
}

void update_oled() {
  if (!oled_ok) return;

  bool data_ok = (millis() - last_data_ms < 5000);

  oled.clearDisplay();

  oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setCursor(10, 2);
  oled.print("VITAL SIGNS MONITOR");

  oled.setTextColor(SSD1306_WHITE);

  if (data_ok) {
    oled.setTextSize(1);
    oled.setCursor(0, 16);
    oled.print("Heart Rate:");
    oled.setTextSize(2);
    oled.setCursor(0, 26);
    if (heart_rate > 0) {
      oled.print(heart_rate);
      oled.setTextSize(1);
      oled.print(" BPM");
    } else {
      oled.print("---");
    }

    oled.setTextSize(1);
    oled.setCursor(0, 44);
    oled.print("SpO2:");
    oled.setTextSize(2);
    oled.setCursor(35, 44);
    if (spo2_level > 0) {
      oled.print(spo2_level);
      oled.setTextSize(1);
      oled.print("%");
    } else {
      oled.print("---");
    }

    if (critical_state) {
      oled.fillRect(0, 56, 128, 8, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setTextSize(1);
      oled.setCursor(22, 57);
      oled.print("!! CRITICAL !!");
      oled.setTextColor(SSD1306_WHITE);
    } else {
      oled.setTextSize(1);
      oled.setCursor(42, 57);
      oled.print("NORMAL");
    }

  } else {
    oled.setTextSize(1);
    oled.setCursor(20, 28);
    oled.print("Waiting for data");
    oled.setCursor(25, 42);
    oled.print("from sensor...");
  }
  oled.display();
}

void handle_alarms() {
  if (critical_state && alarm_enabled) {
    if (millis() - last_alarm_ms >= 500) {
      buzzer_state = !buzzer_state;
      digitalWrite(BUZZER_PIN, buzzer_state);
      digitalWrite(LED_PIN,    buzzer_state);
      last_alarm_ms = millis();
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN,    LOW);
    buzzer_state = false;
  }
}

void check_data_timeout() {
  if (heart_rate > 0 && millis() - last_data_ms > 5000) {
    Serial.println("[WARN] No data from ESP32-1 for 5s, check connection");
    heart_rate = 0;
    spo2_level = 0;
  }
}

unsigned long last_display_ms = 0;

void loop() {
  process_uart_data();
  process_usb_commands();
  check_data_timeout();

  if (millis() - last_display_ms >= 300) {
    last_display_ms = millis();
    update_lcd();
    update_oled();
  }
  handle_alarms();
}
#define HEARTBEAT_PIN   35    // S3  — Analog input (ADC1_CH7)
#define UART2_RX_PIN    16    // D0  — Receive from ESP32-2
#define UART2_TX_PIN    17    // D1  — Transmit to   ESP32-2

#define HR_CRITICAL_HIGH    120   // BPM above this → critical
#define HR_CRITICAL_LOW     45    // BPM below this → critical
#define SPO2_CRITICAL_LOW   90    // SpO2 below this → critical

#define BEAT_THRESHOLD      2000  // ADC value for peak (0–4095)
#define MIN_BEAT_GAP_MS     300   // Min ms between valid beats (~200 BPM max)
#define BPM_SAMPLE_COUNT    8     // Average over 8 beat intervals

unsigned long beatTimes[BPM_SAMPLE_COUNT];
int           beatIdx          = 0;
bool          beatBufferFull   = false;
bool          lastReadingHigh  = false;
unsigned long lastBeatMs       = 0;
int           currentBPM       = 0;

int simulateSpO2(int bpm) {
  if (bpm == 0)          return 98;   // no reading yet
  if (bpm < 60)          return 99;
  if (bpm <= 100)        return 98;
  if (bpm <= 110)        return 96;
  if (bpm <= 120)        return 93;
  return 88;                           // very high HR → low SpO2
}

void sampleHeartbeat() {
  int raw = analogRead(HEARTBEAT_PIN);
  bool isHigh = (raw > BEAT_THRESHOLD);
  unsigned long now = millis();

  if (isHigh && !lastReadingHigh) {
    // Rising edge detected
    unsigned long gap = now - lastBeatMs;
    if (gap > MIN_BEAT_GAP_MS) {
      beatTimes[beatIdx] = gap;
      beatIdx = (beatIdx + 1) % BPM_SAMPLE_COUNT;
      if (beatIdx == 0) beatBufferFull = true;
      lastBeatMs = now;
    }
  }
  lastReadingHigh = isHigh;
}

int calculateBPM() {
  int count = beatBufferFull ? BPM_SAMPLE_COUNT : beatIdx;
  if (count < 2) return 0;

  unsigned long total = 0;
  for (int i = 0; i < count; i++) total += beatTimes[i];
  float avg = (float)total / count;
  int bpm = (int)(60000.0f / avg);

  // Clamp to physiological range
  if (bpm < 30 || bpm > 220) return 0;
  return bpm;
}

void setup() {
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);

  analogReadResolution(12);       // 12-bit: 0–4095
  analogSetAttenuation(ADC_11db); // Full 0–3.3V range
  Serial.println("TX → ESP32-2 via Serial2 (GPIO17)");
  Serial.println("Heartbeat sensor on GPIO35 (S3)");
}

unsigned long lastSendMs    = 0;
unsigned long lastBPMCalcMs = 0;

void loop() {
  static unsigned long lastSampleMs = 0;
  if (millis() - lastSampleMs >= 10) {
    lastSampleMs = millis();
    sampleHeartbeat();
  }

  if (millis() - lastBPMCalcMs >= 500) {
    lastBPMCalcMs = millis();
    currentBPM = calculateBPM();
  }

  if (millis() - lastSendMs >= 1000) {
    lastSendMs = millis();

    int hr   = currentBPM;
    int spo2 = simulateSpO2(hr);
    bool critical = (hr > HR_CRITICAL_HIGH ||
                     (hr > 0 && hr < HR_CRITICAL_LOW) ||
                     spo2 < SPO2_CRITICAL_LOW);

    Serial2.print(hr);
    Serial2.print(",");
    Serial2.print(spo2);
    Serial2.print(",");
    Serial2.println(critical ? "1" : "0");

    Serial.print("[TX] HR: ");
    Serial.print(hr);
    Serial.print(" BPM | SpO2: ");
    Serial.print(spo2);
    Serial.print("% | Status: ");
    Serial.println(critical ? " CRITICAL " : " NORMAL ");

    while (Serial2.available()) {
      String cmd = Serial2.readStringUntil('\n');
      cmd.trim();
      Serial.print("[RX from ESP32-2] ");
      Serial.println(cmd);
    }
  }
}
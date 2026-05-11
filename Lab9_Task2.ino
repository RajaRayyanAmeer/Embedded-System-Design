#define TOUCH_PIN 5   // D1 = GPIO5

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  Serial.begin(9600);
}

void loop() {
  int state = digitalRead(TOUCH_PIN);

  if (state == HIGH) {
    Serial.println("Touched!");
  }

  delay(1000);
}
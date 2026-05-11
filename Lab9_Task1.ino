// Built-in LED pin for NodeMCU (ESP8266)
#define LED_BUILTIN 2   // GPIO2 (D4)

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);  // Turn LED ON (active LOW)
  delay(1000);                     // Wait 1 second

  digitalWrite(LED_BUILTIN, HIGH); // Turn LED OFF
  delay(1000);                     // Wait 1 second
}
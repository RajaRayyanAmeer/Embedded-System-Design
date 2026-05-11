// ESP32 #1 - SENDER
// Upload this to the FIRST ESP32
// Uses UART2: TX=GPIO17, RX=GPIO16

#define RXD0 16
#define TXD0 17

void setup() {
  Serial.begin(115200);             // USB Serial (for monitoring)
  Serial2.begin(9600, SERIAL_8N1, RXD0, TXD0); // UART to ESP32 #2
  Serial.println("Sender Ready!");
}

void loop() {
  // Send a message every 2 seconds
  String msg = "Hello from ESP32 #1!";
  Serial2.println(msg);
  Serial.println("Sent: " + msg);

  // Also listen for any reply
  if (Serial2.available()) {
    String reply = Serial2.readStringUntil('\n');
    Serial.println("Reply received: " + reply);
  }

  delay(2000);
}
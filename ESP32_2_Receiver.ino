// ESP32 #2 - RECEIVER
// Upload this to the SECOND ESP32
// Uses UART2: TX=GPIO17, RX=GPIO16

#define RXD0 16
#define TXD0 17

void setup() {
  Serial.begin(115200);             // USB Serial (for monitoring)
  Serial2.begin(9600, SERIAL_8N1, RXD0, TXD0); // UART from ESP32 #1
  Serial.println("Receiver Ready!");
}

void loop() {
  // Listen for incoming message
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    Serial.println("Received: " + incoming);

    // Send a reply back
    String reply = "Got it! - ESP32 #2";
    Serial2.println(reply);
    Serial.println("Replied: " + reply);
  }
}
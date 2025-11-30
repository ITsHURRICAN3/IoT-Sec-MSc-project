#include <WiFi.h>
#include <WiFiClient.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

WiFiClient client;
IPAddress serverIP(192,168,4,1); 
uint16_t port = 3333;

String bufferInput = "";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nConnesso all'AP.");

  while (!client.connect(serverIP, port)) {
    Serial.println("Tentativo connessione fallito...");
    delay(1000);
  }

  Serial.println("Connesso al server!");
}

void loop() {

  while (client.available()) {
    String s = client.readStringUntil('\n');
    s.trim();
    Serial.println(s);
  }

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (bufferInput.length() > 0) {
        client.println(bufferInput);
        bufferInput = "";
      }
    } else {
      bufferInput += c;
    }
  }

  if (!client.connected()) {
    Serial.println("Connessione persa. Riconnessione...");
    while (!client.connect(serverIP, port)) {
      delay(1000);
    }
    Serial.println("Riconnesso.");
  }

  delay(10);
}

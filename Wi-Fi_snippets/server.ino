#include <WiFi.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

WiFiServer server(3333);

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  Serial.print("AP attivo. IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("Client connesso!");

    while (client.connected()) {
      if (client.available()) {
        String data = client.readStringUntil('\n');
        Serial.print("Ricevuto: ");
        Serial.println(data);
        client.println("OK: " + data);
      }
    }

    client.stop();
    Serial.println("Client disconnesso.");
  }
}

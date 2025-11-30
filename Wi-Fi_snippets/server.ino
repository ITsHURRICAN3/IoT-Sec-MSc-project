#include <WiFi.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

WiFiServer server(3333);
WiFiClient client;  // client persistente

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  Serial.print("AP attivo. IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}

void loop() {

  // Se non c'è ancora un client, prova ad accettarlo
  if (!client || !client.connected()) {
    client = server.available();

    if (client) {
      Serial.println("Client connesso!");
    }

    delay(50);
    return;
  }

  // Se invece è connesso, gestisco i dati sulla connessione attiva
  if (client.available()) {
    String data = client.readStringUntil('\n');
    data.trim();

    Serial.print("Ricevuto: ");
    Serial.println(data);

    client.println("ACK");
  }

  delay(10);
}
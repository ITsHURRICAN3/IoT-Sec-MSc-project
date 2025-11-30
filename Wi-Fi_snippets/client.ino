#include <WiFi.h>
#include <WiFiClient.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

WiFiClient client;
uint16_t port = 3333;
IPAddress serverIP(192,168,4,1);  // SoftAP ESP32 ha sempre questo IP

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connessione all'AP...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nConnesso all'AP!");
  Serial.print("IP client: ");
  Serial.println(WiFi.localIP());

  // Prima connessione al server
  Serial.println("Connessione al server...");
  while (!client.connect(serverIP, port)) {
    Serial.println("Tentativo fallito, retry...");
    delay(1000);
  }

  Serial.println("Connesso al server!");
}

void loop() {

  // Se la connessione cade, tenta di riconnettersi
  if (!client.connected()) {
    Serial.println("Connessione persa. Riconnessione...");
    while (!client.connect(serverIP, port)) {
      Serial.println("Retry...");
      delay(1000);
    }
    Serial.println("Riconnesso!");
  }

  // Invio periodico ogni 3 secondi
  if (millis() - lastSend > 3000) {
    lastSend = millis();

    client.println("12345");
    Serial.println("Inviato: 12345");
  }

  // Lettura dell'ACK se arriva
  if (client.available()) {
    String risposta = client.readStringUntil('\n');
    risposta.trim();

    Serial.print("Risposta: ");
    Serial.println(risposta);
  }

  delay(10);
}

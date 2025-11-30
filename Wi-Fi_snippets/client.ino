#include <WiFi.h>
#include <WiFiClient.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

IPAddress serverIP;
uint16_t port = 3333;

bool trovaServer() {
  Serial.println("Scansione rete per trovare il server...");

  // L’AP ESP32 usa sempre rete 192.168.4.x
  for (int i = 1; i < 255; i++) {
    IPAddress testIP(192, 168, 4, i);

    // Tentativo di connessione rapida (timeout breve)
    WiFiClient temp;
    if (temp.connect(testIP, port)) {
      serverIP = testIP;
      temp.stop();
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connessione all'AP...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nConnesso all'AP!");

  if (trovaServer()) {
    Serial.print("Server trovato: ");
    Serial.println(serverIP);
  } else {
    Serial.println("Server NON trovato.");
  }
}

void loop() {
  WiFiClient client;

  if (!client.connect(serverIP, port)) {
    Serial.println("Connessione al server fallita.");
    delay(2000);
    return;
  }

  client.println("12345");
  Serial.println("Inviato: 12345");

  if (client.available()) {
    String risposta = client.readStringUntil('\n');
    Serial.print("Risposta: ");
    Serial.println(risposta);
  }

  client.stop();
  delay(3000);
}

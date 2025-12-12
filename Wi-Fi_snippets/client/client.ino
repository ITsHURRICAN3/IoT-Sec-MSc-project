#include <WiFi.h>
#include <WiFiClient.h>

WiFiClient client;
IPAddress serverIP(192,168,4,1);
uint16_t port = 3333;

String bufferInput = "";
String userInput = "";
String ssid = "";
String password = "";

bool waitingSSID = true;
bool waitingPWD = false;
bool wifiConnected = false;
bool setupMode = true;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Inserire SSID della rete WiFi:");
}

void loop() {

  // ----------------------------
  // FASE 1: RACCOLTA SSID/PWD
  // ----------------------------
  if (setupMode) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (userInput.length() > 0) {
          processInput(userInput);
          userInput = "";
        }
      } else {
        userInput += c;
      }
    }
    return;
  }

  // ----------------------------
  // FASE 2: CLIENT OPERATIVO
  // ----------------------------

  // Ricezione dal server
  while (client.available()) {
    String s = client.readStringUntil('\n');
    s.trim();
    Serial.println(s);
  }

  // Invio verso server
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

  // Riconnessione TCP in caso di caduta
  if (!client.connected()) {
    Serial.println("Connessione al server persa. Riconnessione...");
    while (!client.connect(serverIP, port)) {
      delay(1000);
    }
    Serial.println("Riconnesso.");
  }

  delay(10);
}

void processInput(String input) {
  if (waitingSSID) {
    ssid = input;
    waitingSSID = false;
    waitingPWD = true;
    Serial.println("SSID ricevuto. Inserire password:");
    return;
  }

  if (waitingPWD) {
    password = input;
    waitingPWD = false;
    connectToWiFi();
  }
}

void connectToWiFi() {
  Serial.print("Connessione a ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nConnessione fallita. Riavviare per riprovare.");
    while (true);
  }

  Serial.println("\nConnesso al WiFi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  wifiConnected = true;

  Serial.println("Connessione al server...");
  while (!client.connect(serverIP, port)) {
    Serial.println("Tentativo fallito...");
    delay(1000);
  }

  Serial.println("Connesso al server!");

  // Ora la serial serve solo per comunicare col server
  setupMode = false;
}

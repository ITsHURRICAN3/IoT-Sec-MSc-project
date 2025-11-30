#include <WiFi.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

WiFiServer server(3333);
WiFiClient client;

enum StatoSessione {
  MENU,
  REGISTRAZIONE_EMAIL,
  REGISTRAZIONE_PASSWORD,
  LOGIN_EMAIL,
  LOGIN_PASSWORD
};

StatoSessione stato = MENU;

void inviaMenu() {
  client.println("\n=== MENU PRINCIPALE ===");
  client.println("1) Registrazione");
  client.println("2) Login");
  client.println("Seleziona un'opzione:");
}

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  Serial.print("AP attivo con IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}

void loop() {
  
  // Gestione connessione persistente
  if (!client || !client.connected()) {
    client = server.available();

    if (client) {
      Serial.println("Client connesso!");
      stato = MENU;
      inviaMenu();
    }
    return;
  }

  if (client.available()) {
    String input = client.readStringUntil('\n');
    input.trim();

    switch (stato) {

      case MENU:
        if (input == "1") {
          stato = REGISTRAZIONE_EMAIL;
          client.println("Inserisci email per la registrazione:");
        } 
        else if (input == "2") {
          stato = LOGIN_EMAIL;
          client.println("Inserisci email per il login:");
        } 
        else {
          client.println("Opzione non valida.");
          inviaMenu();
        }
        break;

      case REGISTRAZIONE_EMAIL:
        Serial.print("Registrazione - Email ricevuta: ");
        Serial.println(input);
        stato = REGISTRAZIONE_PASSWORD;
        client.println("Inserisci password:");
        break;

      case REGISTRAZIONE_PASSWORD:
        Serial.print("Registrazione - Password ricevuta: ");
        Serial.println(input);
        client.println("Registrazione completata (simulazione).");
        stato = MENU;
        inviaMenu();
        break;

      case LOGIN_EMAIL:
        Serial.print("Login - Email ricevuta: ");
        Serial.println(input);
        stato = LOGIN_PASSWORD;
        client.println("Inserisci password:");
        break;

      case LOGIN_PASSWORD:
        Serial.print("Login - Password ricevuta: ");
        Serial.println(input);
        client.println("Login completato (simulazione).");
        stato = MENU;
        inviaMenu();
        break;
    }
  }
}

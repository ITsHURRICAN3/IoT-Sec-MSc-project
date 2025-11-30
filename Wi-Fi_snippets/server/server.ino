#include <WiFi.h>
#include <SD.h>
#include <SPI.h>

const char* ssid = "ESP32_Server";
const char* password = "123456789";

WiFiServer server(3333);
WiFiClient client;

int SD_CS_PIN = 5;   // <-- CONTROLLA QUESTO

enum StatoSessione {
  MENU,
  REGISTRAZIONE_EMAIL,
  REGISTRAZIONE_PASSWORD,
  LOGIN_EMAIL,
  LOGIN_PASSWORD
};

StatoSessione stato = MENU;
String tempEmail;

bool salvaCredenziali(String email, String pwd) {
  Serial.println("Apertura file users.txt...");

  File f = SD.open("/users.txt", FILE_APPEND);
  if (!f) {
    Serial.println("ERRORE: impossibile aprire users.txt");
    return false;
  }

  f.println(email + ":" + pwd);
  f.close();

  Serial.println("Credenziali salvate.");
  return true;
}

bool verificaCredenziali(String email, String pwd) {
  Serial.println("Lettura users.txt...");

  File f = SD.open("/users.txt");
  if (!f) {
    Serial.println("ERRORE: impossibile aprire users.txt");
    return false;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    int sep = line.indexOf(':');
    if (sep < 0) continue;

    String e = line.substring(0, sep);
    String p = line.substring(sep + 1);

    if (email == e && pwd == p) {
      f.close();
      return true;
    }
  }

  f.close();
  return false;
}

void inviaMenu() {
  client.println("");
  client.println("=== MENU PRINCIPALE ===");
  client.println("1) Registrazione");
  client.println("2) Login");
  client.println("Seleziona un'opzione:");
}

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  server.begin();

  Serial.println("");
  Serial.println("Inizializzazione SD...");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("ERRORE SD! Verifica cablaggio.");
  } else {
    Serial.println("SD OK.");
  }
}

void loop() {

  if (!client || !client.connected()) {
    client = server.available();

    if (client) {
      Serial.println("Client connesso");
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
          client.println("Inserisci email:");
        } 
        else if (input == "2") {
          stato = LOGIN_EMAIL;
          client.println("Inserisci email:");
        } 
        else {
          client.println("Opzione invalida");
          inviaMenu();
        }
        break;

      case REGISTRAZIONE_EMAIL:
        tempEmail = input;
        stato = REGISTRAZIONE_PASSWORD;
        client.println("Inserisci password:");
        break;

      case REGISTRAZIONE_PASSWORD:
        if (salvaCredenziali(tempEmail, input))
          client.println("Registrazione completata.");
        else
          client.println("Errore salvataggio.");

        stato = MENU;
        inviaMenu();
        break;

      case LOGIN_EMAIL:
        tempEmail = input;
        stato = LOGIN_PASSWORD;
        client.println("Inserisci password:");
        break;

      case LOGIN_PASSWORD:
        if (verificaCredenziali(tempEmail, input))
          client.println("ACK - Login OK");
        else
          client.println("NACK - Credenziali errate");

        stato = MENU;
        inviaMenu();
        break;
    }
  }
}

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 5      // SDA
#define RST_PIN 22    // RST

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  SPI.begin();           // Avvia SPI
  rfid.PCD_Init();       // Avvia RFID
  Serial.println("Lettore RFID pronto. Avvicina un tag...");
}

void loop() {

  // Nessuna nuova carta → esci
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Carta presente ma non leggibile → esci
  if (!rfid.PICC_ReadCardSerial()) {
    Serial.println("Errore nella lettura della carta!");
    return;
  }

  Serial.print("TAG rilevato! UID: ");

  // Stampa l’UID della carta
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Ferma la comunicazione con il tag
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(500); // leggero delay per stabilità
}

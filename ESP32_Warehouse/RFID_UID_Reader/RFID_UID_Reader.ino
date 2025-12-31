/*
 * ESP32 RFID UID Reader
 * 
 * Hardware Requirements:
 * - ESP32 Development Board
 * - MFRC522 RFID Module
 * 
 * Wiring Connections:
 * -----------------------------
 * MFRC522      ESP32
 * -----------------------------
 * SDA (SS)     GPIO 5
 * SCK          GPIO 18
 * MOSI         GPIO 23
 * MISO         GPIO 19
 * RST          GPIO 22
 * 3.3V         3.3V
 * GND          GND
 * -----------------------------
 * 
 * Instructions:
 * 1. Install "MFRC522" library by GithubCommunity in Arduino IDE Library Manager.
 * 2. Upload this sketch.
 * 3. Open Serial Monitor at 115200 baud.
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  5
#define RST_PIN 22

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial to be ready
  
  SPI.begin();     // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

  Serial.println(F("\n\n-----------------------------"));
  Serial.println(F("ESP32 RFID UID Reader"));
  Serial.println(F("Place a card near the reader..."));
  Serial.println(F("-----------------------------\n"));
  
  // Optional: print reader version
  rfid.PCD_DumpVersionToSerial();
}

void loop() {
  // Look for new cards
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("UID tag :"));
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println();
  
  // Halt PICC to stop reading the same card multiple times quickly
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

// Helper routine to dump a byte array as hex values to Serial
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

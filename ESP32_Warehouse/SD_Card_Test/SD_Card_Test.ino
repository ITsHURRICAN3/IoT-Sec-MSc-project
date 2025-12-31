/*
 * ESP32 MicroSD Card Test Script
 *
 * Wiring (VSPI Default):
 * CS   -> GPIO 5
 * SCK  -> GPIO 18
 * MOSI -> GPIO 23
 * MISO -> GPIO 19
 * VCC  -> 5V or 3.3V (Check your module specs)
 * GND  -> GND
 */

#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Define CS pin
// NOTE: If using with RFID on the same SPI bus, ensure this CS is unique
// (e.g., RFID CS on 5, SD CS on 4)
#define SD_CS 5

void setup() {
  Serial.begin(115200);
  while(!Serial); // Wait for serial to be ready

  Serial.println("\n------------------------------");
  Serial.println("ESP32 MicroSD Card Test");
  Serial.println("------------------------------");
  Serial.println("Initializing SD card...");

  // Initialize SD card
  // If your SD card is on VSPI (default), this works.
  // If connection fails, check wiring and power.
  if(!SD.begin(SD_CS)){
    Serial.println("ERROR: Card Mount Failed!");
    Serial.println("Possible causes:");
    Serial.println("1. Wiring incorrect (MISO/MOSI swapped?)");
    Serial.println("2. CS pin incorrect (defined as 5)");
    Serial.println("3. Card not inserted or damaged");
    Serial.println("4. Power supply insufficient");
    return;
  }
  
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  
  // Create a test file
  Serial.println("\nWriting test file...");
  File file = SD.open("/test.txt", FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
  } else {
    if(file.print("Hello from ESP32")){
      Serial.println("File written");
    } else {
      Serial.println("Write failed");
    }
    file.close();
  }

  Serial.println("------------------------------");
  Serial.println("Test Complete");
}

void loop() {
  // Nothing to do here
}

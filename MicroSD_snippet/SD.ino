#include <SPI.h>
#include <SD.h>

// Definisci i pin SPI della SD
#define SD_CS   5   // Chip Select
#define SD_SCK  18  // Clock
#define SD_MISO 19  // MISO
#define SD_MOSI 23  // MOSI

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Inizializzazione SD sulla ESP32...");

  // Inizializza il bus SPI con i pin definiti
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // Inizializza la SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Errore nella inizializzazione della SD Card!");
    return;
  }
  Serial.println("SD inizializzata correttamente.");

  writeFile();
  readFile();
}

void loop() {
  // Vuoto per questo esempio
}

void writeFile() {
  File file = SD.open("/esempio.txt", FILE_WRITE);
  if (file) {
    file.println("Ciao dalla ESP32!");
    file.println("Scrittura su SD card riuscita.");
    file.close();
    Serial.println("File scritto con successo.");
  } else {
    Serial.println("Errore nell'apertura del file!");
  }
}

void readFile() {
  File file = SD.open("/esempio.txt");
  if (file) {
    Serial.println("Contenuto del file:");
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();
  } else {
    Serial.println("Errore nell'apertura del file per la lettura.");
  }
}
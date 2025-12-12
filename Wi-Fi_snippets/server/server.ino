/*
  ESP32: server AP + users file fully re-encrypted on each registration
  Uses libsodium crypto_secretstream_xchacha20poly1305 for streaming encrypt/decrypt.
  - CHUNK sized to 1024
  - static buffers to avoid big stack usage
  - temporary plaintext file used only inside device (removed after use)
*/

#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <sodium.h>

#define CHUNK 1024
#define SD_CS 5

// Demo key: in produzione NON lasciare qui la chiave, usare secure storage
const unsigned char KEY[crypto_secretstream_xchacha20poly1305_KEYBYTES] = {
  0x45,0x91,0xAA,0x1F,0xC3,0x55,0x44,0x10,
  0x21,0x82,0x0C,0x9A,0xDD,0x36,0x11,0x5F,
  0x18,0xBB,0xF2,0x90,0x47,0xF0,0x5A,0x3C,
  0xB2,0xAC,0xE1,0xD0,0x9F,0x00,0x32,0x89
};

// Static buffers
static unsigned char inbuf[CHUNK];
static unsigned char outbuf[CHUNK + crypto_secretstream_xchacha20poly1305_ABYTES];

const char* ENC_FILE = "/users.dat";        // file cifrato sulla SD
const char* TMP_PLAIN = "/.users.tmp";     // file temporaneo plaintext (rimosso subito)

// WiFi AP
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
String tempEmail;

// ---------- helper: write String plaintext to tmp file ----------
bool writePlainTemp(const String &txt) {
  File f = SD.open(TMP_PLAIN, FILE_WRITE);
  if (!f) return false;
  size_t written = f.print(txt);
  f.close();
  return written == txt.length();
}

// ---------- encrypt file (plaintext TMP_PLAIN -> ENC_FILE) ----------
bool encryptTempToEnc() {
  File fin = SD.open(TMP_PLAIN, FILE_READ);
  if (!fin) return false;

  // write to a temp encrypted file (atomic replace)
  const char *tmpEnc = "/.users.enc.tmp";
  File fout = SD.open(tmpEnc, FILE_WRITE);
  if (!fout) { fin.close(); return false; }

  unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
  crypto_secretstream_xchacha20poly1305_state st;
  if (crypto_secretstream_xchacha20poly1305_init_push(&st, header, KEY) != 0) {
    fin.close(); fout.close(); return false;
  }

  // write header
  if (fout.write(header, sizeof(header)) != sizeof(header)) { fin.close(); fout.close(); return false; }

  while (true) {
    int r = fin.read((uint8_t*)inbuf, CHUNK);
    if (r < 0) { fin.close(); fout.close(); return false; }
    if (r == 0) break;

    unsigned long long outlen = 0;
    unsigned char tag = (fin.available() == 0) ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;

    if (crypto_secretstream_xchacha20poly1305_push(&st,
          outbuf, &outlen,
          inbuf, (unsigned long long)r,
          NULL, 0,
          tag) != 0) {
      fin.close(); fout.close(); return false;
    }

    // write len32 LE
    uint32_t len32 = (uint32_t)outlen;
    if (fout.write((uint8_t*)&len32, sizeof(len32)) != sizeof(len32)) { fin.close(); fout.close(); return false; }

    // write cipher bytes
    if (fout.write(outbuf, outlen) != (size_t)outlen) { fin.close(); fout.close(); return false; }

    if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) break;
  }

  fin.close();
  fout.close();

  // replace target file atomically
  SD.remove(ENC_FILE);
  if (!SD.rename("/.users.enc.tmp", ENC_FILE)) {
    // fallback: try copy
    File ftmp = SD.open("/.users.enc.tmp", FILE_READ);
    File fdst = SD.open(ENC_FILE, FILE_WRITE);
    if (!ftmp || !fdst) { if (ftmp) ftmp.close(); if (fdst) fdst.close(); return false; }
    while (ftmp.available()) {
      uint8_t b = ftmp.read();
      fdst.write(b);
    }
    ftmp.close(); fdst.close();
    SD.remove("/.users.enc.tmp");
  }
  // remove tmp plaintext
  SD.remove(TMP_PLAIN);
  return true;
}

// ---------- decrypt ENC_FILE -> return plaintext String ----------
bool decryptEncToString(String &outPlain) {
  outPlain = "";
  File fin = SD.open(ENC_FILE, FILE_READ);
  if (!fin) return false;

  unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
  if (fin.read(header, sizeof(header)) != sizeof(header)) { fin.close(); return false; }

  crypto_secretstream_xchacha20poly1305_state st;
  if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, KEY) != 0) { fin.close(); return false; }

  while (true) {
    // read 4-byte length
    uint32_t len32;
    int rl = fin.read((uint8_t*)&len32, sizeof(len32));
    if (rl == 0) break; // EOF
    if (rl != sizeof(len32)) { fin.close(); return false; }
    if (len32 > sizeof(inbuf)) { fin.close(); return false; }

    // read cipher chunk
    int rr = fin.read(inbuf, len32);
    if (rr != (int)len32) { fin.close(); return false; }

    unsigned long long outlen = 0;
    unsigned char tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(&st,
          outbuf, &outlen, &tag,
          inbuf, len32,
          NULL, 0) != 0) {
      fin.close(); return false;
    }

    // append plaintext chunk to outPlain
    outPlain += String((char*)outbuf).substring(0, (int)outlen);

    if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) break;
  }

  fin.close();
  return true;
}

// ---------- helper: find credentials in plaintext (format email:pwd per riga) ----------
bool checkCredentialsInPlain(const String &plain, const String &email, const String &pwd) {
  int start = 0;
  while (start < (int)plain.length()) {
    int nl = plain.indexOf('\n', start);
    String line;
    if (nl < 0) { line = plain.substring(start); start = plain.length(); }
    else { line = plain.substring(start, nl); start = nl + 1; }
    line.trim();
    if (line.length() == 0) continue;
    int sep = line.indexOf(':');
    if (sep < 0) continue;
    String e = line.substring(0, sep);
    String p = line.substring(sep + 1);
    if (e == email && p == pwd) return true;
  }
  return false;
}

// ---------- load plaintext string; if encrypted file missing, return empty string ----------
bool loadUsersPlain(String &outPlain) {
  if (!SD.exists(ENC_FILE)) { outPlain = ""; return true; }
  return decryptEncToString(outPlain);
}

// ---------- save plaintext string by writing tmp and encrypting ----------
bool saveUsersFromPlain(const String &plain) {
  // write tmp plaintext
  if (!writePlainTemp(plain)) return false;
  return encryptTempToEnc();
}

// ---------- append one "email:pwd\n" to users by full decrypt->append->encrypt ----------
bool addUser(const String &email, const String &pwd) {
  String plain;
  if (!loadUsersPlain(plain)) return false;
  if (plain.length() > 0 && plain.charAt(plain.length()-1) != '\n') plain += "\n";
  plain += email;
  plain += ":";
  plain += pwd;
  plain += "\n";
  return saveUsersFromPlain(plain);
}

bool verifyUser(const String &email, const String &pwd) {
  String plain;
  if (!loadUsersPlain(plain)) return false;
  return checkCredentialsInPlain(plain, email, pwd);
}

// ---------- networking/menu ----------
void inviaMenu() {
  client.println("");
  client.println("=== MENU PRINCIPALE ===");
  client.println("1) Registrazione");
  client.println("2) Login");
  client.println("Seleziona un'opzione:");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("Init SD...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD.begin fallita. Controlla wiring e CS pin.");
    while (1) delay(1000);
  }
  Serial.println("SD OK.");

  if (sodium_init() < 0) {
    Serial.println("sodium_init fallita");
    while (1) delay(1000);
  }

  // WiFi AP + server
  WiFi.softAP(ssid, password);
  server.begin();
  Serial.print("AP attivo. IP: ");
  Serial.println(WiFi.softAPIP());

  // ensure encrypted file exists: if not, create empty encrypted file
  if (!SD.exists(ENC_FILE)) {
    Serial.println("users.dat inesistente: creo file cifrato vuoto...");
    // save empty plaintext
    if (!saveUsersFromPlain(String(""))) {
      Serial.println("Impossibile creare file cifrato iniziale");
      while (1) delay(1000);
    }
    Serial.println("File cifrato creato.");
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
    delay(50);
    return;
  }

  if (client.available()) {
    String input = client.readStringUntil('\n');
    input.trim();

    switch (stato) {
      case MENU:
        if (input == "1") {
          stato = REGISTRAZIONE_EMAIL;
          client.println("Inserisci email per registrazione:");
        } else if (input == "2") {
          stato = LOGIN_EMAIL;
          client.println("Inserisci email per login:");
        } else {
          client.println("Opzione invalida");
          inviaMenu();
        }
        break;

      case REGISTRAZIONE_EMAIL:
        tempEmail = input;
        stato = REGISTRAZIONE_PASSWORD;
        client.println("Inserisci password:");
        break;

      case REGISTRAZIONE_PASSWORD: {
        String pwd = input;
        client.println("Sto salvando le credenziali...");
        if (addUser(tempEmail, pwd)) {
          client.println("Registrazione completata.");
          Serial.printf("Nuovo utente registrato: %s\n", tempEmail.c_str());
        } else {
          client.println("Errore nel salvataggio.");
        }
        stato = MENU;
        inviaMenu();
        break;
      }

      case LOGIN_EMAIL:
        tempEmail = input;
        stato = LOGIN_PASSWORD;
        client.println("Inserisci password:");
        break;

      case LOGIN_PASSWORD: {
        String pwd = input;
        bool ok = verifyUser(tempEmail, pwd);
        if (ok) {
          client.println("ACK - Login OK");
          Serial.printf("Login OK: %s\n", tempEmail.c_str());
        } else {
          client.println("NACK - Credenziali errate");
          Serial.printf("Login FAIL: %s\n", tempEmail.c_str());
        }
        stato = MENU;
        inviaMenu();
        break;
      }
    }
  }
}
//
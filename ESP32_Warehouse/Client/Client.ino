#include <MFRC522.h>
#include <SPI.h>
#include <Sodium.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char *SERVER_IP = "192.168.4.1";
const int SERVER_PORT = 443;

// RFID Pins
#define SS_PIN 5
#define RST_PIN 22

MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClientSecure client; // TLS Client

// --- ROOT CA CERTIFICATE (TRUST ANCHOR) ---
// IMPORTANT: REPLACE THIS with your generated CA Certificate
// Questo certificato serve a verificare che il Server sia chi dice di essere.
// Deve corrispondere alla CA che ha firmato il certificato del Server.
const char *root_ca_cert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDpTCCAo2gAwIBAgIUA0YJxHkK/dEcB+fj8IDMzwc+FDYwDQYJKoZIhvcNAQEL\n"
    "BQAwYjELMAkGA1UEBhMCSVQxETAPBgNVBAgMCEF2ZWxsaW5vMREwDwYDVQQHDAhB\n"
    "dmVsbGlubzENMAsGA1UECgwEd2FyZDENMAsGA1UECwwEd2FyZDEPMA0GA1UEAwwG\n"
    "TXlSb290MB4XDTI2MDEyMjE1NDQzNFoXDTM2MDEyMDE1NDQzNFowYjELMAkGA1UE\n"
    "BhMCSVQxETAPBgNVBAgMCEF2ZWxsaW5vMREwDwYDVQQHDAhBdmVsbGlubzENMAsG\n"
    "A1UECgwEd2FyZDENMAsGA1UECwwEd2FyZDEPMA0GA1UEAwwGTXlSb290MIIBIjAN\n"
    "BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsioNP32gklNVc+DF+8FXuCcnJ3xZ\n"
    "y64mQaydd4aSZQDQA98JhNJvkmnt9sqfvYK7/xoec5ZI97JXpEDFXtfAHuVhw8Ni\n"
    "2Yy15k47IZn/OU1PsBcXtLW3uzDog2nbR6lVgPyOshtkPgeJSuXdMrs0jPzmu0+3\n"
    "uyYlJZng4bQxfwuakP0hEV6AfE/6M7pSuSr35GmCTYfYSFOpxex3jgS9vaYbveqr\n"
    "b641PssyQ1jmT7qzAmBshNBLXj0y4a4nzoakIzmUGHgh6ojSsj5vsNV9mRkgTwVE\n"
    "hOoXhtwKxGYB439BeNfaGaFYLayAxKSSU1QMMl4cbn3HtwX7EzQrRzKcewIDAQAB\n"
    "o1MwUTAdBgNVHQ4EFgQUUCRyfkIeJlmx0vZ3PSFB6JEpTMwwHwYDVR0jBBgwFoAU\n"
    "UCRyfkIeJlmx0vZ3PSFB6JEpTMwwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0B\n"
    "AQsFAAOCAQEAqAFlpsXHfDrk+xpmZOmda8mrMvfzlmZc/6tpYOoAuVBYJxg0iJFG\n"
    "rwvuFSXjxcTMVPzxobANxtaZ//kZw6wfp8di5pNJXi4wqyqCz8cmUd9tWAHrxvHF\n"
    "jB89EyBfXKWXoM7QQq9S1JScq7pYu/fqtHGQAWGZ6nC7tDq0FV07I8Ps1MBPiA1Y\n"
    "4QYWANqWKBDy/+A2snTwqtIqBgClDBPFnTGCm98DF2QRyx1LgUTS90HPW9uRk7BE\n"
    "u92vAUuSUmEgWDQ3KDpV1Xmm7CsMMseEmP77t7PilWY6MilgBWW9okpjhZhwPlYV\n"
    "ZbLdPG294sg4yfHoFGiTifKNP0iDi1eYdw==\n"
    "-----END CERTIFICATE-----\n";

// Prototypes
void connect();
void printAuthHelp();

// --- Helper: Wait for RFID and return UID String ---
// String getRFIDUID() {
//   Serial.println(">> Please scan your RFID Tag now..."); // Cleaned per user
//   request
String getRFIDUID() {
  // Silent scan

  while (true) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String uid = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10)
          uid += "0";
        uid += String(mfrc522.uid.uidByte[i], HEX);
      }
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      uid.toUpperCase();
      return uid;
    }
    delay(50);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init SPI & RFID
  SPI.begin();
  mfrc522.PCD_Init();

  if (sodium_init() < 0) {
    Serial.println("Sodium Init Failed!");
    while (1)
      ;
  }

  // Load Trust Anchor (CA)
  client.setCACert(root_ca_cert);

  Serial.println("\n--- ESP32 Client Setup (TLS Enabled) ---");
  Serial.println("RFID Reader Initialized.");

  // --- HACK PER RETE ISOLATA ---
  // Impostiamo l'ora manualmente a una data valida (es. 1 Gen 2025)
  // Così la validazione temporale del certificato X.509 passa
  struct timeval tv;
  tv.tv_sec = 1735689600; // Epoch per 1 Gen 2025
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  Serial.println("Time manually set to 2025 (fake) for Cert Validation.");

  // Start Connection
  connect();
}

// --- User Identity ---
unsigned char user_pk[crypto_sign_PUBLICKEYBYTES];
unsigned char user_sk[crypto_sign_SECRETKEYBYTES];
bool hasIdentity = false;

// Derive Ed25519 Keypair from UID + Password
void deriveIdentity(String uid, String pass) {
  unsigned char seed[crypto_sign_SEEDBYTES];
  String input = uid + pass;

  // Use generic hash (BLAKE2b) to create deterministic seed from credentials
  crypto_generichash(seed, sizeof(seed), (const unsigned char *)input.c_str(),
                     input.length(), NULL, 0);

  // Derive Keypair
  crypto_sign_seed_keypair(user_pk, user_sk, seed);
  hasIdentity = true;

  Serial.println("Identity Derived.");
}

void connect() {
  while (true) {
    WiFi.disconnect(true);
    delay(100);

    // 1. WiFi Connection
    while (WiFi.status() != WL_CONNECTED) {
      while (Serial.available())
        Serial.read(); // Clear junk

      Serial.println("Enter WiFi SSID:");
      while (Serial.available() == 0) {
        delay(100);
      }
      String ssid = Serial.readStringUntil('\n');
      ssid.trim();
      // Serial.println("SSID: " + ssid); // Removed

      Serial.println("Enter WiFi Password:");
      while (Serial.available() == 0) {
        delay(100);
      }
      String pass = Serial.readStringUntil('\n');
      pass.trim();
      // Serial.println("Password: " + pass); // Removed

      Serial.print("Connecting to WiFi...");
      WiFi.begin(ssid.c_str(), pass.c_str());

      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("\nFailed to connect. Retrying...");
        WiFi.disconnect(true);
        delay(1000);
      }
    }

    // 2. Connect to Server (SECURE)
    Serial.print("Connecting to Secure Server at ");
    Serial.print(SERVER_IP);
    Serial.println("...");

    // TLS Handshake happens here!
    // It will verify the Server Cert against root_ca_cert
    // Note: Use setCACert() in setup()
    if (client.connect(SERVER_IP, SERVER_PORT)) {
      Serial.println("Connected to Secure Server (Verified)!");
      Serial.println("Transport Layer: TLS 1.2/1.3");
    } else {
      Serial.println("Connection Failed! (Check Certs or IP)");
      char err_buf[100];
      client.lastError(err_buf, 100);
      Serial.print("TLS Error Code: ");
      Serial.println(err_buf);
    }

    if (client.connected()) {
      // Main Loop inside function or return to main loop?
      // Let's use the main loop() to handle traffic
      return;
    }

    // If we reach here, connection failed. Loop restarts.
    delay(1000);
  }
}

void loop() {
  if (!client.connected()) {
    Serial.println("Disconnected from server.");
    client.stop();
    connect();
  }

  // --- Read from Server (Decrypted by TLS) ---
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    // Serial.println("[RX]: " + line); // Debug Removed/Silenced
    // Only print if NOT a challenge (cleaner UI)
    bool likelyChallenge = (line.length() == 64);
    if (!likelyChallenge)
      Serial.println(line); // Print Server Msg (Menu, etc)

    // Check logic for authentication errors
    // if (line.indexOf("Error: User") != -1 || line.startsWith("Auth Failed"))
    // {
    //   // Menu is sent by server, no need to print local help
    // }

    // Challenge Response Logic (App Layer Auth)
    // Check if this is a Challenge (Random Hex String of 64 chars)
    if (line.length() == 64 && hasIdentity) {
      bool isHex = true;
      for (char c : line) {
        if (!isxdigit(c))
          isHex = false;
      }

      if (isHex) {
        Serial.println("Received Challenge! Generating Proof...");
        unsigned char challenge[32];
        sodium_hex2bin(challenge, sizeof(challenge), line.c_str(),
                       line.length(), NULL, NULL, NULL);

        unsigned char sig[crypto_sign_BYTES];
        crypto_sign_detached(sig, NULL, challenge, sizeof(challenge), user_sk);

        char sigHex[crypto_sign_BYTES * 2 + 1];
        sodium_bin2hex(sigHex, sizeof(sigHex), sig, sizeof(sig));

        // Send Plaintext (Protected by TLS)
        client.println(String(sigHex));
        Serial.println("Proof Sent.");
      }
    }
  }

  // --- Read from Serial (Send to Server) ---
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      // Intercept REG and LOG to inject PK
      if (input.startsWith("REG ")) {
        int sp1 = input.indexOf(' ');
        int sp2 = input.indexOf(' ', sp1 + 1);
        if (sp2 != -1) {
          String u = input.substring(sp1 + 1, sp2);
          String p = input.substring(sp2 + 1);

          // Get Real RFID UID
          Serial.println("Scan RFID Tag to bind to the new user");
          String uid = getRFIDUID();
          // Serial.println("Tag Read: " + uid); // Removed

          // Derive Identity
          deriveIdentity(uid, p);

          // Generate UID Hash for Uniqueness Check
          unsigned char uidHash[crypto_generichash_BYTES];
          crypto_generichash(uidHash, sizeof(uidHash),
                             (const unsigned char *)uid.c_str(), uid.length(),
                             NULL, 0);
          char uidHashHex[crypto_generichash_BYTES * 2 + 1];
          sodium_bin2hex(uidHashHex, sizeof(uidHashHex), uidHash,
                         sizeof(uidHash));

          char pkHex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
          sodium_bin2hex(pkHex, sizeof(pkHex), user_pk, sizeof(user_pk));

          // Format: REG username pk_hex uid_hash_hex
          String cmd =
              "REG " + u + " " + String(pkHex) + " " + String(uidHashHex);
          client.println(cmd); // TLS Send
          Serial.println("Sent Registration Data (TLS Encrypted).");
        } else {
          Serial.println("Use: REG user pass");
        }
      } else if (input.startsWith("LOG ")) {
        int sp1 = input.indexOf(' ');
        int sp2 = input.indexOf(' ', sp1 + 1);
        if (sp2 != -1) {
          String u = input.substring(sp1 + 1, sp2);
          String p = input.substring(sp2 + 1);

          Serial.println("Scan RFID Tag for Login...");
          String uid = getRFIDUID();

          deriveIdentity(uid, p);

          char pkHex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
          sodium_bin2hex(pkHex, sizeof(pkHex), user_pk, sizeof(user_pk));

          String cmd = "LOG " + u + " " + String(pkHex);
          client.println(cmd); // TLS Send
          Serial.println("Login Requested (TLS Encrypted).");
        } else {
          Serial.println("Use: LOG user pass");
        }
      } else {
        // Just forward command (NEW ENTRY etc.)
        client.println(input);
      }
    }
  }
}

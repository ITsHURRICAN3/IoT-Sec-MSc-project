#include <WiFi.h>
#include <Sodium.h>
#include <SPI.h>
#include <MFRC522.h>

const char* SERVER_IP = "192.168.4.1";
const int SERVER_PORT = 80;

// RFID Pins
#define SS_PIN  5
#define RST_PIN 22

MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClient client;

// Crypto Globals
unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
unsigned char client_sk[crypto_kx_SECRETKEYBYTES];
unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
unsigned char rx[crypto_kx_SESSIONKEYBYTES];
unsigned char tx[crypto_kx_SESSIONKEYBYTES];
bool isSecure = false;

// --- Helper: Wait for RFID and return UID String ---
String getRFIDUID() {
  Serial.println(">> Please scan your RFID Tag now...");
  while (true) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String uid = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(mfrc522.uid.uidByte[i], HEX);
      }
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      uid.toUpperCase();
      return uid;
    }
    delay(50);
    // Keep serial alive/inputs? No, pure block for RFID
  }
}

// --- Crypto Helpers ---

void sendEncrypted(String msg) {
  if (!isSecure) {
    client.println(msg);
    return;
  }
  
  unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
  randombytes_buf(nonce, sizeof(nonce));

  unsigned long long ciphertext_len;
  int msg_len = msg.length();
  unsigned char* ciphertext = (unsigned char*) malloc(msg_len + crypto_aead_chacha20poly1305_ietf_ABYTES);

  crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, &ciphertext_len,
                                            (const unsigned char*)msg.c_str(), msg_len,
                                            NULL, 0,
                                            NULL, nonce, tx);

  char* hex_nonce = (char*) malloc(sizeof(nonce) * 2 + 1);
  sodium_bin2hex(hex_nonce, sizeof(nonce) * 2 + 1, nonce, sizeof(nonce));

  char* hex_cipher = (char*) malloc(ciphertext_len * 2 + 1);
  sodium_bin2hex(hex_cipher, ciphertext_len * 2 + 1, ciphertext, ciphertext_len);

  client.print(hex_nonce);
  client.print(":");
  client.println(hex_cipher);

  free(ciphertext);
  free(hex_nonce);
  free(hex_cipher);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Init SPI & RFID
  SPI.begin();
  mfrc522.PCD_Init();
  
  if (sodium_init() < 0) {
    Serial.println("Sodium Init Failed!");
    while(1);
  }

  Serial.println("\n--- ESP32 Client Setup (Secure + RFID) ---");
  Serial.println("RFID Reader Initialized.");
  
  // 1. WiFi Connection
  while (WiFi.status() != WL_CONNECTED) {
    while (Serial.available()) Serial.read(); // Clear junk

    Serial.println("Enter WiFi SSID:");
    while (Serial.available() == 0) { delay(100); }
    String ssid = Serial.readStringUntil('\n');
    ssid.trim();
    Serial.println("SSID: " + ssid);

    Serial.println("Enter WiFi Password:");
    while (Serial.available() == 0) { delay(100); }
    String pass = Serial.readStringUntil('\n');
    pass.trim();
    Serial.println("Password: " + pass);

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
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to connect. Retrying...");
      WiFi.disconnect(true);
      delay(1000);
    }
  }

  // 2. Connect to Server
  Serial.print("Connecting to Server at ");
  Serial.print(SERVER_IP);
  Serial.println("...");

  if (client.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println("Connected to TCP Server.");
    
    // 3. Secure Handshake
    SessionHandshake();
    
  } else {
    Serial.println("Connection failed.");
    while(1);
  }
}

void SessionHandshake() {
  Serial.println("Starting Secure Handshake...");
  
  // 1. Generate Ephemeral Keys
  crypto_kx_keypair(client_pk, client_sk);
  
  // 2. Send Client PK
  char hex_client_pk[crypto_kx_PUBLICKEYBYTES * 2 + 1];
  sodium_bin2hex(hex_client_pk, sizeof(hex_client_pk), client_pk, sizeof(client_pk));
  client.println(hex_client_pk);
  
  // 3. Wait for Server PK
  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      
      if (line.length() == crypto_kx_PUBLICKEYBYTES * 2) {
         sodium_hex2bin(server_pk, sizeof(server_pk), line.c_str(), line.length(), NULL, NULL, NULL);
         
         // 4. Compute Session Keys
         if (crypto_kx_client_session_keys(rx, tx, client_pk, client_sk, server_pk) != 0) {
            Serial.println("Key Exchange Failed.");
            while(1);
         }
         
         isSecure = true;
         Serial.println("Secure Handshake Success!");
         return;
      }
    }
  }
  Serial.println("Handshake Timeout.");
  while(1);
}

// --- User Identity ---
unsigned char user_pk[crypto_sign_PUBLICKEYBYTES];
unsigned char user_sk[crypto_sign_SECRETKEYBYTES];
bool hasIdentity = false;

// Derive Ed25519 Keypair from UID + Password (simulating ZKP Secret)
void deriveIdentity(String uid, String pass) {
  unsigned char seed[crypto_sign_SEEDBYTES];
  unsigned char hash_in[128]; // Arbitrary buffer
  String input = uid + pass;
  
  // Use generic hash (BLAKE2b) to create deterministic seed from credentials
  crypto_generichash(seed, sizeof(seed), (const unsigned char*)input.c_str(), input.length(), NULL, 0);
  
  // Derive Keypair
  crypto_sign_seed_keypair(user_pk, user_sk, seed);
  hasIdentity = true;
  
  Serial.println("Identity Derived (ZKP Secret Ready).");
}

void loop() {
  if (!client.connected()) {
    Serial.println("Disconnected from server.");
    while(1); 
  }

  // --- Read from Server ---
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    
    int sep = line.indexOf(':');
    if (sep != -1) {
       // Decrypt
       String nonceHex = line.substring(0, sep);
       String cipherHex = line.substring(sep + 1);
       unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
       sodium_hex2bin(nonce, sizeof(nonce), nonceHex.c_str(), nonceHex.length(), NULL, NULL, NULL);
       int cipherLen = cipherHex.length() / 2;
       unsigned char* ciphertext = (unsigned char*) malloc(cipherLen);
       sodium_hex2bin(ciphertext, cipherLen, cipherHex.c_str(), cipherHex.length(), NULL, NULL, NULL);
       unsigned char* decrypted = (unsigned char*) malloc(cipherLen - crypto_aead_chacha20poly1305_ietf_ABYTES + 1);
       unsigned long long decrypted_len;

       if (crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, &decrypted_len, NULL, ciphertext, cipherLen, NULL, 0, nonce, rx) == 0) {
           decrypted[decrypted_len] = '\0';
           String plain = String((char*)decrypted);
           Serial.println("RX: " + plain);
           
           // Check if this is a Challenge (Random Hex String of 64 chars = 32 bytes)
           if (plain.length() == 64 && hasIdentity) {
              bool isHex = true;
              for (char c : plain) { if (!isxdigit(c)) isHex = false; }
              
              if (isHex) {
                 Serial.println("Received Challenge! Genering ZKP Proof...");
                 unsigned char challenge[32];
                 sodium_hex2bin(challenge, sizeof(challenge), plain.c_str(), plain.length(), NULL, NULL, NULL);
                 
                 unsigned char sig[crypto_sign_BYTES];
                 crypto_sign_detached(sig, NULL, challenge, sizeof(challenge), user_sk);
                 
                 char sigHex[crypto_sign_BYTES * 2 + 1];
                 sodium_bin2hex(sigHex, sizeof(sigHex), sig, sizeof(sig));
                 
                 sendEncrypted(String(sigHex));
                 Serial.println("Proof Sent.");
              }
           }
           
       } else {
           Serial.println("[Decryption Error]");
       }
       free(ciphertext);
       free(decrypted);
    } else {
       Serial.println("[RAW]: " + line);
    }
  }

  // --- Read from Serial ---
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      if (isSecure) {
         // Intercept REG and LOG to inject PK or Handle Flow
         if (input.startsWith("REG ")) {
            int sp1 = input.indexOf(' ');
            int sp2 = input.indexOf(' ', sp1 + 1);
            if (sp2 != -1) {
              String u = input.substring(sp1 + 1, sp2);
              String p = input.substring(sp2 + 1);
              
              // Get Real RFID UID
              Serial.println("Scan RFID Tag to bind to User: " + u);
              String uid = getRFIDUID();
              Serial.println("Tag Read: " + uid);
              
              deriveIdentity(uid, p); 
              
              char pkHex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
              sodium_bin2hex(pkHex, sizeof(pkHex), user_pk, sizeof(user_pk));
              
              String cmd = "REG " + u + " " + String(pkHex);
              sendEncrypted(cmd);
              Serial.println("Sent Public Key for Registration.");
            } else {
               Serial.println("Use: REG user pass");
            }
         } else if (input.startsWith("LOG ")) {
            int sp1 = input.indexOf(' ');
            int sp2 = input.indexOf(' ', sp1 + 1); 
            if (sp2 != -1) {
               String u = input.substring(sp1 + 1, sp2);
               String p = input.substring(sp2 + 1);
               
               // Get Real RFID UID
               Serial.println("Scan RFID Tag for Login...");
               String uid = getRFIDUID();
               Serial.println("Tag Read: " + uid);
               
               deriveIdentity(uid, p);
               
               String cmd = "LOG " + u; 
               sendEncrypted(cmd);
               Serial.println("Login Requested. Waiting for Challenge...");
            } else {
               Serial.println("Use: LOG user pass"); 
            }
         } else {
            sendEncrypted(input);
         }
      } else {
         client.println(input);
      }
    }
  }
}

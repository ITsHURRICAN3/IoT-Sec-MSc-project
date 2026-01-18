#include <SD.h>
#include <SPI.h>
#include <Sodium.h> // LibSodium
#include <WiFi.h>

// --- Configuration ---
const char *AP_SSID = "ESP_Server_AP";
const char *AP_PASS = "12345678";
const int SERVER_PORT = 80;
const int MAX_QTY = 10000;

// SD Card Pins (Default HSPI)
const int SD_CS_PIN = 5;

// Files
const char *FILE_USERS = "/users.txt";
const char *FILE_WAREHOUSE = "/warehouse.txt";

// --- Globals ---
WiFiServer server(SERVER_PORT);

// Session State
enum State { STATE_HANDSHAKE, STATE_AUTH, STATE_LOGGED_IN };

// Crypto Globals
unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
unsigned char server_sk[crypto_kx_SECRETKEYBYTES];
unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
unsigned char rx[crypto_kx_SESSIONKEYBYTES]; // Receive key
unsigned char tx[crypto_kx_SESSIONKEYBYTES]; // Transmit key
bool isSecure = false;

// Storage Key (Simulated Secure Element)
unsigned char storage_key[crypto_aead_chacha20poly1305_ietf_KEYBYTES];

struct ClientSession {
  State state;
  String username;
};

// --- Helper Functions ---

// Validator for Name (User or Product): Alphanumeric, 5-50 chars
bool isValidName(String s) {
  if (s.length() < 5 || s.length() > 50)
    return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isAlphaNumeric(s.charAt(i)))
      return false;
  }
  return true;
}

// Validator for Number
bool isValidNumber(String s) {
  if (s.length() == 0)
    return false;
  unsigned int start = 0;
  if (s.charAt(0) == '-' || s.charAt(0) == '+')
    start = 1;
  if (start == s.length())
    return false;
  for (unsigned int i = start; i < s.length(); i++) {
    if (!isDigit(s.charAt(i)))
      return false;
  }
  return true;
}

// Initialize SD Card
bool initSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed");
    return false;
  }

  // Set fixed storage key for academic demo (normally from Secure Boot / Efuse)
  // "AcademicSecureKey123456789012345" (32 bytes)
  const char *k = "AcademicSecureKey123456789012345";
  memcpy(storage_key, k, sizeof(storage_key));

  // Create files if they don't exist
  if (!SD.exists(FILE_USERS)) {
    File f = SD.open(FILE_USERS, FILE_WRITE);
    if (f)
      f.close();
  }
  if (!SD.exists(FILE_WAREHOUSE)) {
    File f = SD.open(FILE_WAREHOUSE, FILE_WRITE);
    if (f)
      f.close();
  }
  return true;
}

// --- Crypto Storage Helpers ---

String encryptLine(String plain) {
  unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
  randombytes_buf(nonce, sizeof(nonce));

  unsigned long long ciphertext_len;
  int msg_len = plain.length();
  unsigned char *ciphertext = (unsigned char *)malloc(
      msg_len + crypto_aead_chacha20poly1305_ietf_ABYTES);

  crypto_aead_chacha20poly1305_ietf_encrypt(
      ciphertext, &ciphertext_len, (const unsigned char *)plain.c_str(),
      msg_len, NULL, 0, NULL, nonce, storage_key);

  char *hex_nonce = (char *)malloc(sizeof(nonce) * 2 + 1);
  sodium_bin2hex(hex_nonce, sizeof(nonce) * 2 + 1, nonce, sizeof(nonce));

  char *hex_cipher = (char *)malloc(ciphertext_len * 2 + 1);
  sodium_bin2hex(hex_cipher, ciphertext_len * 2 + 1, ciphertext,
                 ciphertext_len);

  String res = String(hex_nonce) + ":" + String(hex_cipher);

  free(ciphertext);
  free(hex_nonce);
  free(hex_cipher);
  return res;
}

String decryptLine(String line) {
  int sep = line.indexOf(':');
  if (sep == -1)
    return ""; // Not encrypted or invalid

  String nonceHex = line.substring(0, sep);
  String cipherHex = line.substring(sep + 1);

  unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
  sodium_hex2bin(nonce, sizeof(nonce), nonceHex.c_str(), nonceHex.length(),
                 NULL, NULL, NULL);

  int cipherLen = cipherHex.length() / 2;
  unsigned char *ciphertext = (unsigned char *)malloc(cipherLen);
  sodium_hex2bin(ciphertext, cipherLen, cipherHex.c_str(), cipherHex.length(),
                 NULL, NULL, NULL);

  unsigned char *decrypted = (unsigned char *)malloc(
      cipherLen - crypto_aead_chacha20poly1305_ietf_ABYTES + 1);
  unsigned long long decrypted_len;

  String res = "";
  if (crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, &decrypted_len, NULL,
                                                ciphertext, cipherLen, NULL, 0,
                                                nonce, storage_key) == 0) {
    decrypted[decrypted_len] = '\0';
    res = String((char *)decrypted);
  }

  free(ciphertext);
  free(decrypted);
  return res;
}

// Get User Public Key (Hex) if exists
// Returns empty string if not found
String getUserPK(String u) {
  File file = SD.open(FILE_USERS);
  if (!file)
    return "";

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    // Decrypt Line
    String plain = decryptLine(line);
    if (plain.length() == 0)
      continue;

    int sep = plain.indexOf(';');
    if (sep == -1)
      continue;

    String f_user = plain.substring(0, sep);
    String f_pk = plain.substring(sep + 1);

    if (f_user == u) {
      file.close();
      return f_pk;
    }
  }
  file.close();
  return "";
}

// Helper to check if a specific tag hash is already registered
bool isTagRegistered(String tagHash) {
  File file = SD.open(FILE_USERS);
  if (!file)
    return false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    String plain = decryptLine(line);
    if (plain.length() == 0)
      continue;

    // Format: username;pk_hex;tag_hash
    int sep1 = plain.indexOf(';');
    if (sep1 == -1)
      continue;
    int sep2 = plain.indexOf(';', sep1 + 1);

    if (sep2 != -1) {
      String storedHash = plain.substring(sep2 + 1);
      if (storedHash == tagHash) {
        file.close();
        return true;
      }
    }
  }
  file.close();
  return false;
}

// Stores encrypted: username;pk_hex;tag_hash
bool registerUserPK(String u, String pk_hex, String tag_hash) {
  if (getUserPK(u) != "")
    return false; // User already exists

  if (isTagRegistered(tag_hash))
    return false; // Tag already registered

  File file = SD.open(FILE_USERS, FILE_APPEND);
  if (!file)
    return false;

  String plain = u + ";" + pk_hex + ";" + tag_hash;
  file.println(encryptLine(plain));
  file.close();
  return true;
}

// Helper to get item quantity. Returns -1 if not found.
int getItemQty(String targetItem) {
  File file = SD.open(FILE_WAREHOUSE);
  if (!file)
    return -1;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    String plain = decryptLine(line);
    if (plain.length() == 0)
      continue;

    int sep = plain.indexOf(';');
    if (sep != -1) {
      String currentItem = plain.substring(0, sep);
      if (currentItem == targetItem) {
        String qtyStr = plain.substring(sep + 1);
        file.close();
        return qtyStr.toInt();
      }
    }
  }
  file.close();
  return -1;
}

// --- CRUD Operations ---

String readWarehouse() {
  File file = SD.open(FILE_WAREHOUSE);
  if (!file)
    return "Error opening warehouse file.\n";

  String output = "\n--- Warehouse Inventory ---\n";
  bool empty = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      String plain = decryptLine(line);
      if (plain.length() > 0) {
        int sep = plain.indexOf(';');
        if (sep != -1) {
          String i = plain.substring(0, sep);
          String q = plain.substring(sep + 1);
          output += i + " -> " + q + "\n";
          empty = false;
        }
      }
    }
  }
  file.close();
  if (empty)
    output += "(Empty)\n";
  return output;
}

bool addItem(String item, int qty) {
  File file = SD.open(FILE_WAREHOUSE, FILE_APPEND);
  if (!file)
    return false;
  String plain = item + ";" + String(qty);
  file.println(encryptLine(plain));
  file.close();
  return true;
}

// Helper to rewrite file excluding a specific item or updating it
// mode: 0=delete, 1=update
bool modifyItem(String targetItem, int newQty, int mode) {
  File file = SD.open(FILE_WAREHOUSE);
  if (!file)
    return false;

  String tempContent = "";
  bool found = false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    String plain = decryptLine(line);
    if (plain.length() == 0)
      continue;

    int sep = plain.indexOf(';');
    if (sep != -1) {
      String currentItem = plain.substring(0, sep);

      if (currentItem == targetItem) {
        found = true;
        if (mode == 1) { // Update
          String newPlain = currentItem + ";" + String(newQty);
          tempContent += encryptLine(newPlain) + "\n";
        }
        // If mode == 0 (Delete), we skip adding it
      } else {
        // Keep existing encrypted line (re-writing it is safe)
        tempContent += line + "\n";
      }
    }
  }
  file.close();

  if (!found && mode == 1)
    return false; // Item to update not found

  // Rewrite file
  SD.remove(FILE_WAREHOUSE);
  file = SD.open(FILE_WAREHOUSE, FILE_WRITE);
  if (!file)
    return false;
  file.print(tempContent);
  file.close();

  return true;
}

// --- Crypto Helpers ---

// Send Encrypted Message: [NONCE_HEX]:[CIPHERTEXT_HEX]
void sendEncrypted(WiFiClient &client, String msg) {
  if (!isSecure) {
    client.println(msg);
    return;
  }

  unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
  randombytes_buf(nonce, sizeof(nonce));

  unsigned long long ciphertext_len;
  int msg_len = msg.length();
  unsigned char *ciphertext = (unsigned char *)malloc(
      msg_len + crypto_aead_chacha20poly1305_ietf_ABYTES);

  crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, &ciphertext_len,
                                            (const unsigned char *)msg.c_str(),
                                            msg_len, NULL, 0, // No AD
                                            NULL, nonce, tx);

  char *hex_nonce = (char *)malloc(sizeof(nonce) * 2 + 1);
  sodium_bin2hex(hex_nonce, sizeof(nonce) * 2 + 1, nonce, sizeof(nonce));

  char *hex_cipher = (char *)malloc(ciphertext_len * 2 + 1);
  sodium_bin2hex(hex_cipher, ciphertext_len * 2 + 1, ciphertext,
                 ciphertext_len);

  client.print(hex_nonce);
  client.print(":");
  client.println(hex_cipher);

  free(ciphertext);
  free(hex_nonce);
  free(hex_cipher);
}

// Reset Session for new client
void resetSecurity() {
  isSecure = false;
  // Generate new ephemeral keys for this session
  crypto_kx_keypair(server_pk, server_sk);
}

// --- Main Setup & Loop ---

void setup() {
  Serial.begin(115200);

  if (sodium_init() < 0) {
    Serial.println("Sodium Init Failed!");
    while (1)
      ;
  }

  // Init SD
  if (!initSD()) {
    Serial.println("SD Init Failed! Halting.");
    while (1)
      ;
  }
  Serial.println("SD Card Initialized.");

  // Init WiFi AP
  WiFi.softAP(AP_SSID, AP_PASS, 1, 1);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Start Server
  server.begin();
  Serial.println("Server started (Secure Mode).");
}

// Helper to send menu with a status prefix
void sendMenu(WiFiClient &client, String prefix = "") {
  String menu = "";
  if (prefix.length() > 0) {
    menu += prefix + "\n\n";
  }
  menu += "--- MENU ---\n";
  menu += "1. NEW ENTRY item qty\n";
  menu += "2. READ\n";
  menu += "3. UPDATE item new_qty\n";
  menu += "4. ADD item qty_to_add\n";
  menu += "5. SUB item qty_to_sub\n";
  menu += "6. DELETE item\n";
  menu += "7. LOGOUT";
  sendEncrypted(client, menu);
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client Connected.");
    resetSecurity(); // Gen new ephemeral keys

    // Handshake Phase
    unsigned long start = millis();
    bool handshakeSuccess = false;

    while (client.connected() && millis() - start < 5000) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();

        if (line.length() == crypto_kx_PUBLICKEYBYTES * 2) {
          // Decode Client Public Key
          sodium_hex2bin(client_pk, sizeof(client_pk), line.c_str(),
                         line.length(), NULL, NULL, NULL);

          // Compute Session Keys
          if (crypto_kx_server_session_keys(rx, tx, server_pk, server_sk,
                                            client_pk) != 0) {
            Serial.println("Key Exchange Failed");
            client.stop();
            break;
          }

          // Send Server Public Key
          char hex_server_pk[crypto_kx_PUBLICKEYBYTES * 2 + 1];
          sodium_bin2hex(hex_server_pk, sizeof(hex_server_pk), server_pk,
                         sizeof(server_pk));
          client.println(hex_server_pk);

          isSecure = true;
          handshakeSuccess = true;
          Serial.println("Secure Handshake Complete.");
          break;
        }
      }
    }

    if (!handshakeSuccess) {
      Serial.println("Handshake Timeout or Fail.");
      client.stop();
      return;
    }

    ClientSession session;
    session.state = STATE_AUTH;

    // Secure Loop
    sendEncrypted(client, "Welcome to ESP32 Warehouse (Secure)!");
    sendEncrypted(client, "1. Register (Format: REG username password)\n2. "
                          "Login (Format: LOG username password)");

    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
          continue;

        // Decrypt
        int sep = line.indexOf(':');
        if (sep == -1)
          continue;

        String nonceHex = line.substring(0, sep);
        String cipherHex = line.substring(sep + 1);

        unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
        sodium_hex2bin(nonce, sizeof(nonce), nonceHex.c_str(),
                       nonceHex.length(), NULL, NULL, NULL);

        int cipherLen = cipherHex.length() / 2;
        unsigned char *ciphertext = (unsigned char *)malloc(cipherLen);
        sodium_hex2bin(ciphertext, cipherLen, cipherHex.c_str(),
                       cipherHex.length(), NULL, NULL, NULL);

        unsigned char *decrypted = (unsigned char *)malloc(
            cipherLen - crypto_aead_chacha20poly1305_ietf_ABYTES + 1);
        unsigned long long decrypted_len;

        if (crypto_aead_chacha20poly1305_ietf_decrypt(
                decrypted, &decrypted_len, NULL, ciphertext, cipherLen, NULL, 0,
                nonce, rx) != 0) {
          Serial.println("Decryption Failed!");
          free(ciphertext);
          free(decrypted);
          continue;
        }
        decrypted[decrypted_len] = '\0';
        String plain = String((char *)decrypted);
        Serial.println("RX: " + plain);

        free(ciphertext);
        free(decrypted);

        // Command Processing
        if (session.state == STATE_AUTH) {
          if (plain.startsWith("REG ")) {
            // REG username hex_public_key hex_tag_hash
            int gap1 = plain.indexOf(' ');
            int gap2 = plain.indexOf(' ', gap1 + 1);
            int gap3 = plain.indexOf(' ', gap2 + 1);

            if (gap2 != -1 && gap3 != -1) {
              String u = plain.substring(gap1 + 1, gap2);
              String pk_hex = plain.substring(gap2 + 1, gap3);
              String tag_hash = plain.substring(gap3 + 1);

              if (!isValidName(u)) {
                sendEncrypted(client,
                              "Error: Invalid Username (5-50 Alphanumeric).");
              } else if (pk_hex.length() != crypto_sign_PUBLICKEYBYTES * 2) {
                sendEncrypted(client, "Error: Invalid Public Key length.");
              } else {
                if (isTagRegistered(tag_hash)) {
                  sendEncrypted(client, "Error: Tag already registered!");
                } else if (registerUserPK(u, pk_hex, tag_hash)) {
                  sendEncrypted(client, "REG SUCCESS");
                } else {
                  sendEncrypted(client, "Error: User exists or Write Fail");
                }
              }
            } else
              sendEncrypted(
                  client, "Invalid Format. Use: REG username pk_hex tag_hash");

            sendEncrypted(client,
                          "1. Register (Format: REG username password)\n2. "
                          "Login (Format: LOG username password)");
          } else if (plain.startsWith("LOG ")) {
            // LOG username
            // 1. Get User PK
            int gap1 = plain.indexOf(' ');
            String u = plain.substring(gap1 + 1);

            String user_pk_hex;
            if (isValidName(u)) {
              user_pk_hex = getUserPK(u);
            } else {
              user_pk_hex = "";
            }

            if (user_pk_hex == "") {
              if (!isValidName(u))
                sendEncrypted(client, "Error: Invalid Username Format.");
              else
                sendEncrypted(client, "Error: User not found.");
            } else {
              // 2. Generate Challenge (Nonce)
              unsigned char challenge[32];
              randombytes_buf(challenge, sizeof(challenge));
              char challenge_hex[65];
              sodium_bin2hex(challenge_hex, sizeof(challenge_hex), challenge,
                             sizeof(challenge));

              // 3. Send Challenge
              sendEncrypted(client, String(challenge_hex));

              // 4. Wait for Signature
              unsigned long authStart = millis();
              bool authDone = false;

              while (client.connected() && millis() - authStart < 5000) {
                if (client.available()) {
                  String line = client.readStringUntil('\n');
                  line.trim();
                  if (line.length() == 0)
                    continue;

                  // Decrypt Signature
                  String sigHex = "";
                  // ... (decrypt logic inline or re-use helper? Inline for now
                  // to avoid complexity)
                  int sep = line.indexOf(':');
                  if (sep != -1) {
                    String nonceHex = line.substring(0, sep);
                    String cipherHex = line.substring(sep + 1);
                    unsigned char
                        n[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
                    sodium_hex2bin(n, sizeof(n), nonceHex.c_str(),
                                   nonceHex.length(), NULL, NULL, NULL);
                    int cLen = cipherHex.length() / 2;
                    unsigned char *c = (unsigned char *)malloc(cLen);
                    sodium_hex2bin(c, cLen, cipherHex.c_str(),
                                   cipherHex.length(), NULL, NULL, NULL);
                    unsigned char *d = (unsigned char *)malloc(
                        cLen - crypto_aead_chacha20poly1305_ietf_ABYTES + 1);
                    unsigned long long dLen;
                    if (crypto_aead_chacha20poly1305_ietf_decrypt(
                            d, &dLen, NULL, c, cLen, NULL, 0, n, rx) == 0) {
                      d[dLen] = '\0';
                      sigHex = String((char *)d);
                    }
                    free(c);
                    free(d);
                  }

                  if (sigHex.length() > 0) {
                    // 5. Verify Signature
                    unsigned char sig[crypto_sign_BYTES];
                    sodium_hex2bin(sig, sizeof(sig), sigHex.c_str(),
                                   sigHex.length(), NULL, NULL, NULL);

                    unsigned char user_pk[crypto_sign_PUBLICKEYBYTES];
                    sodium_hex2bin(user_pk, sizeof(user_pk),
                                   user_pk_hex.c_str(), user_pk_hex.length(),
                                   NULL, NULL, NULL);

                    if (crypto_sign_verify_detached(
                            sig, challenge, sizeof(challenge), user_pk) == 0) {
                      session.state = STATE_LOGGED_IN;
                      session.username = u;
                      sendMenu(client, "Login Successful! (ZKP Verified)");
                    } else {
                      sendEncrypted(client, "Auth Failed: Invalid Signature.");
                    }
                    authDone = true;
                    break;
                  }
                }
              }
              if (!authDone)
                sendEncrypted(client, "Auth Timeout.");
            }
          } else
            sendEncrypted(client, "Unknown command. Please Register or Login.");

        } else if (session.state == STATE_LOGGED_IN) {
          if (plain.startsWith("NEW ENTRY ")) {
            String rest = plain.substring(10); // "NEW ENTRY "
            int sp = rest.lastIndexOf(' ');
            if (sp != -1) {
              String item = rest.substring(0, sp);
              String qtyStr = rest.substring(sp + 1);

              if (!isValidName(item)) {
                sendEncrypted(client, "Error: Invalid Name (5-50 Alphanum).");
              } else if (!isValidNumber(qtyStr)) {
                sendEncrypted(client, "Error: Invalid Quantity.");
              } else {
                long qty = qtyStr.toInt();
                if (qty < 0)
                  sendEncrypted(client, "Error: Negative.");
                else if (qty > MAX_QTY)
                  sendEncrypted(client,
                                "Error: Limit of 10000 units exceeded.");
                else if (getItemQty(item) != -1)
                  sendEncrypted(client, "Error: Exists.");
                else {
                  addItem(item, (int)qty);
                  sendMenu(client, "New Entry Added.");
                }
              }
            } else
              sendEncrypted(client, "Invalid Format.");

          } else if (plain == "READ") {
            String resp = readWarehouse();
            sendMenu(client, resp); // Print Inventory then Menu

          } else if (plain.startsWith("UPDATE ")) {
            int sp1 = plain.indexOf(' ');
            int sp2 = plain.lastIndexOf(' ');
            if (sp2 > sp1) {
              String item = plain.substring(sp1 + 1, sp2);
              String qtyStr = plain.substring(sp2 + 1);

              if (!isValidName(item)) {
                sendEncrypted(client, "Error: Invalid Name.");
              } else if (!isValidNumber(qtyStr)) {
                sendEncrypted(client, "Error: Invalid Quantity.");
              } else {
                long qty = qtyStr.toInt();
                if (qty < 0)
                  sendEncrypted(client, "Error: Negative.");
                else if (qty > MAX_QTY)
                  sendEncrypted(client, "Error: Limit of 10000 units exeeded");
                else if (getItemQty(item) == -1)
                  sendEncrypted(client, "Error: Not Found.");
                else {
                  modifyItem(item, (int)qty, 1);
                  sendMenu(client, "Updated.");
                }
              }
            } else
              sendEncrypted(client, "Invalid Format.");

          } else if (plain.startsWith("ADD ")) {
            int sp1 = plain.indexOf(' ');
            int sp2 = plain.lastIndexOf(' ');
            if (sp2 > sp1) {
              String item = plain.substring(sp1 + 1, sp2);
              String qtyStr = plain.substring(sp2 + 1);

              if (!isValidName(item)) {
                sendEncrypted(client, "Error: Invalid command");
              } else if (!isValidNumber(qtyStr)) {
                sendEncrypted(client, "Error: Invalid Quantity.");
              } else {
                long qtyA = qtyStr.toInt();
                int q = getItemQty(item);
                if (q == -1)
                  sendEncrypted(client, "Error: Not Found.");
                else if (qtyA < 0)
                  sendEncrypted(client, "Error: Negative.");
                else if ((long)q + qtyA > MAX_QTY)
                  sendEncrypted(client, "Error: Limit of 10000 units exeeded");
                else {
                  modifyItem(item, q + (int)qtyA, 1);
                  sendMenu(client, "Added.");
                }
              }
            } else
              sendEncrypted(client, "Invalid Format.");

          } else if (plain.startsWith("SUB ")) {
            int sp1 = plain.indexOf(' ');
            int sp2 = plain.lastIndexOf(' ');
            if (sp2 > sp1) {
              String item = plain.substring(sp1 + 1, sp2);
              String qtyStr = plain.substring(sp2 + 1);

              if (!isValidName(item)) {
                sendEncrypted(client, "Error: Invalid Name.");
              } else if (!isValidNumber(qtyStr)) {
                sendEncrypted(client, "Error: Invalid Quantity.");
              } else {
                long qtyS = qtyStr.toInt();
                int q = getItemQty(item);
                if (q == -1)
                  sendEncrypted(client, "Error: Non-existent item");
                else if (q - qtyS < 0)
                  sendEncrypted(client, "Error: Quantity below 0");
                else {
                  modifyItem(item, q - (int)qtyS, 1);
                  sendMenu(client, "Subtracted.");
                }
              }
            } else
              sendEncrypted(client, "Invalid Format.");

          } else if (plain.startsWith("DELETE ")) {
            String item = plain.substring(7);
            if (!isValidName(item)) {
              sendEncrypted(client, "Error: Invalid Name.");
            } else if (getItemQty(item) != -1) {
              modifyItem(item, 0, 0);
              sendMenu(client, "Deleted.");
            } else
              sendEncrypted(client, "Error: Not Found.");

          } else if (plain == "LOGOUT") {
            session.state = STATE_AUTH;
            sendEncrypted(client, "\nLogged out.\n");
            sendEncrypted(client,
                          "1. Register (Format: REG username password)\n2. "
                          "Login (Format: LOG username password)");
          } else
            sendEncrypted(client, "Unknown Command.");
        }
      }
    }
  }
}

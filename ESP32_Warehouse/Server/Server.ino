#include <WiFi.h>
#include <SD.h>
#include <SPI.h>

// --- Configuration ---
const char* AP_SSID = "ESP_Server_AP";
const char* AP_PASS = "12345678";
const int SERVER_PORT = 80;
const int MAX_QTY = 10000;

// SD Card Pins (Default HSPI)
const int SD_CS_PIN = 5;

// Files
const char* FILE_USERS = "/users.txt";
const char* FILE_WAREHOUSE = "/warehouse.txt";

// --- Globals ---
WiFiServer server(SERVER_PORT);

// Session State
enum State {
  STATE_AUTH,
  STATE_LOGGED_IN
};

struct ClientSession {
  State state;
  String username;
};

// --- Helper Functions ---

// Initialize SD Card
bool initSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed");
    return false;
  }
  
  // Create files if they don't exist
  if (!SD.exists(FILE_USERS)) {
    File f = SD.open(FILE_USERS, FILE_WRITE);
    if (f) f.close();
  }
  if (!SD.exists(FILE_WAREHOUSE)) {
    File f = SD.open(FILE_WAREHOUSE, FILE_WRITE);
    if (f) f.close();
  }
  return true;
}

// Check if user exists and password matches
// Returns: 0 = fail, 1 = success, 2 = user already exists (for registration)
int checkUser(String u, String p, bool isLogin) {
  File file = SD.open(FILE_USERS);
  if (!file) return 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sep = line.indexOf(';');
    if (sep == -1) continue;

    String f_user = line.substring(0, sep);
    String f_pass = line.substring(sep + 1);

    if (f_user == u) {
      if (isLogin) {
        if (f_pass == p) {
          file.close();
          return 1; // Login success
        }
      } else {
        file.close();
        return 2; // Register fail: User exists
      }
    }
  }
  file.close();
  return isLogin ? 0 : 1; // Login fail (not found) or Register success (new)
}

bool registerUser(String u, String p) {
  if (checkUser(u, p, false) == 2) return false; // Already exists

  File file = SD.open(FILE_USERS, FILE_APPEND);
  if (!file) return false;

  file.println(u + ";" + p);
  file.close();
  return true;
}

// Helper to get item quantity. Returns -1 if not found.
int getItemQty(String targetItem) {
  File file = SD.open(FILE_WAREHOUSE);
  if (!file) return -1;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sep = line.indexOf(';');
    if (sep != -1) {
      String currentItem = line.substring(0, sep);
      if (currentItem == targetItem) {
        String qtyStr = line.substring(sep + 1);
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
  if (!file) return "Error opening warehouse file.\n";

  String output = "--- Warehouse Inventory ---\n";
  bool empty = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int sep = line.indexOf(';');
      if (sep != -1) {
        String i = line.substring(0, sep);
        String q = line.substring(sep + 1);
        output += i + " -> " + q + "\n";
        empty = false;
      }
    }
  }
  file.close();
  if (empty) output += "(Empty)\n";
  return output;
}

bool addItem(String item, int qty) {
  File file = SD.open(FILE_WAREHOUSE, FILE_APPEND);
  if (!file) return false;
  file.println(item + ";" + String(qty));
  file.close();
  return true;
}

// Helper to rewrite file excluding a specific item or updating it
// mode: 0=delete, 1=update
bool modifyItem(String targetItem, int newQty, int mode) {
  File file = SD.open(FILE_WAREHOUSE);
  if (!file) return false;

  String tempContent = "";
  bool found = false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sep = line.indexOf(';');
    if (sep != -1) {
      String currentItem = line.substring(0, sep);
      
      if (currentItem == targetItem) {
        found = true;
        if (mode == 1) { // Update
          tempContent += currentItem + ";" + String(newQty) + "\n";
        }
        // If mode == 0 (Delete), we skip adding it
      } else {
        tempContent += line + "\n";
      }
    }
  }
  file.close();

  if (!found && mode == 1) return false; // Item to update not found

  // Rewrite file
  SD.remove(FILE_WAREHOUSE);
  file = SD.open(FILE_WAREHOUSE, FILE_WRITE);
  if (!file) return false;
  file.print(tempContent);
  file.close();
  
  return true;
}


// --- Main Setup & Loop ---

void setup() {
  Serial.begin(115200);
  
  // Init SD
  if (!initSD()) {
    Serial.println("SD Init Failed! Halting.");
    while(1);
  }
  Serial.println("SD Card Initialized.");

  // Init WiFi AP
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Start Server
  server.begin();
  Serial.println("Server started.");
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client Connected.");
    ClientSession session;
    session.state = STATE_AUTH;
    
    // Initial Menu
    client.println("Welcome to ESP32 Warehouse!");
    client.println("1. Register (Format: REG username password)");
    client.println("2. Login (Format: LOG username password)");

    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        Serial.println("Received: " + line);

        if (session.state == STATE_AUTH) {
          if (line.startsWith("REG ")) {
            int firstSpace = line.indexOf(' ');
            int secondSpace = line.indexOf(' ', firstSpace + 1);
            if (secondSpace != -1) {
              String u = line.substring(firstSpace + 1, secondSpace);
              String p = line.substring(secondSpace + 1);
              if (registerUser(u, p)) {
                client.println("Registration Successful! Please Login.");
              } else {
                client.println("Registration Failed (User exists or SD error).");
              }
            } else {
              client.println("Invalid Format. Use: REG username password");
            }
          } else if (line.startsWith("LOG ")) {
            int firstSpace = line.indexOf(' ');
            int secondSpace = line.indexOf(' ', firstSpace + 1);
            if (secondSpace != -1) {
              String u = line.substring(firstSpace + 1, secondSpace);
              String p = line.substring(secondSpace + 1);
              if (checkUser(u, p, true)) {
                session.state = STATE_LOGGED_IN;
                session.username = u;
                client.println("Login Successful!");
                client.println("--- MENU ---");
                client.println("1. NEW ENTRY item qty");
                client.println("2. READ");
                client.println("3. UPDATE item new_qty");
                client.println("4. ADD item qty_to_add");
                client.println("5. SUB item qty_to_sub");
                client.println("6. DELETE item");
                client.println("7. LOGOUT");
              } else {
                client.println("Login Failed. Invalid credentials.");
              }
            } else {
              client.println("Invalid Format. Use: LOG username password");
            }
          } else {
             client.println("Unknown command. Please Register or Login.");
          }
        } else if (session.state == STATE_LOGGED_IN) {
          if (line.startsWith("NEW ENTRY ")) {
            // NEW ENTRY item qty
            // Format: "NEW ENTRY <item> <qty>"
            // Since "NEW ENTRY" has a space, we parse carefully
            String prefix = "NEW ENTRY ";
            String rest = line.substring(prefix.length());
            int spaceIndex = rest.lastIndexOf(' '); 
            
            if (spaceIndex != -1) {
               String item = rest.substring(0, spaceIndex);
               String qtyStr = rest.substring(spaceIndex + 1);
               int qty = qtyStr.toInt();
               
               if (qty < 0) {
                 client.println("Error: Quantity cannot be negative."); 
               } else if (qty > MAX_QTY) {
                 client.println("Error: Quantity exceeds limit (" + String(MAX_QTY) + ").");
               } else if (getItemQty(item) != -1) {
                 client.println("Error: Item '" + item + "' already exists. Use ADD/UPDATE.");
               } else {
                 if (addItem(item, qty)) client.println("New Entry Added.");
                 else client.println("Error adding item.");
               }
            } else {
              client.println("Invalid Format. Use: NEW ENTRY item qty");
            }

          } else if (line == "READ") {
            client.print(readWarehouse());

          } else if (line.startsWith("UPDATE ")) {
             // UPDATE item qty
            int firstSpace = line.indexOf(' ');
            int secondSpace = line.lastIndexOf(' ');
            if (secondSpace > firstSpace) {
               String item = line.substring(firstSpace + 1, secondSpace);
               int qty = line.substring(secondSpace + 1).toInt();
               

               
               if (qty < 0) {
                 client.println("Error: Quantity cannot be negative.");
               } else if (qty > MAX_QTY) {
                 client.println("Error: Quantity exceeds limit (" + String(MAX_QTY) + ").");
               } else if (getItemQty(item) == -1) {
                 client.println("Error: Item not found.");
               } else {
                 if (modifyItem(item, qty, 1)) client.println("Item Updated.");
                 else client.println("Error updating item.");
               }
            } else {
              client.println("Invalid Format. Use: UPDATE item qty");
            }

          } else if (line.startsWith("ADD ")) {
            // ADD item qty (AddToExisting)
            int firstSpace = line.indexOf(' ');
            int secondSpace = line.lastIndexOf(' ');
            if (secondSpace > firstSpace) {
               String item = line.substring(firstSpace + 1, secondSpace);
               int qtyToAdd = line.substring(secondSpace + 1).toInt();
               
               if (qtyToAdd < 0) {
                 client.println("Error: Cannot add negative quantity. Use SUB.");
               } else if (qtyToAdd > MAX_QTY) {
                  client.println("Error: Quantity to add exceeds limit (" + String(MAX_QTY) + ").");
               } else {
                 int currentQty = getItemQty(item);
                 if (currentQty == -1) {
                   client.println("Error: Item not found. Use NEW ENTRY.");
                 } else {
                   // Safe overflow check
                   if (MAX_QTY - currentQty < qtyToAdd) {
                      client.println("Error: Total quantity would exceed limit (" + String(MAX_QTY) + ").");
                   } else {
                      int newQty = currentQty + qtyToAdd;
                      if (modifyItem(item, newQty, 1)) client.println("Item Quantity Increased to " + String(newQty) + ".");
                      else client.println("Error updating item.");
                   }
                 }
               }
            } else {
              client.println("Invalid Format. Use: ADD item qty");
            }

          } else if (line.startsWith("SUB ")) {
            // SUB item qty
            int firstSpace = line.indexOf(' ');
            int secondSpace = line.lastIndexOf(' ');
            if (secondSpace > firstSpace) {
               String item = line.substring(firstSpace + 1, secondSpace);
               int qtyToSub = line.substring(secondSpace + 1).toInt();
               
               int currentQty = getItemQty(item);
               if (currentQty == -1) {
                 client.println("Error: Item not found.");
               } else {
                 if (currentQty - qtyToSub < 0) {
                   client.println("Error: Insufficient quantity. Current: " + String(currentQty));
                 } else {
                   int newQty = currentQty - qtyToSub;
                   if (modifyItem(item, newQty, 1)) client.println("Item Quantity Decreased to " + String(newQty) + ".");
                   else client.println("Error updating item.");
                 }
               }
            } else {
              client.println("Invalid Format. Use: SUB item qty");
            }

          } else if (line.startsWith("DELETE ")) {
            // DELETE item
            int firstSpace = line.indexOf(' ');
            String item = line.substring(firstSpace + 1);
            if (getItemQty(item) != -1) { // Check existence first for better feedback
                 if (modifyItem(item, 0, 0)) client.println("Item Deleted.");
                 else client.println("Error deleting item.");
            } else {
                client.println("Error: Item not found.");
            }
            
          } else if (line == "LOGOUT") {
            session.state = STATE_AUTH;
            client.println("Logged out.");
            client.println("1. Register (Format: REG username password)");
            client.println("2. Login (Format: LOG username password)");
          } else {
            client.println("Unknown Command.");
          }
        }
        
        // Re-print prompt or menu hint if needed, but simple response is okay for now.
        client.print("> "); // Prompt
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

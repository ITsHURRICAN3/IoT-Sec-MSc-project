#include <WiFi.h>

const char* SERVER_IP = "192.168.4.1";
const int SERVER_PORT = 80;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- ESP32 Client Setup ---");
  
  while (WiFi.status() != WL_CONNECTED) {
    // Clear buffer to remove garbage
    while (Serial.available()) Serial.read();

    // 1. Get WiFi Credentials
    Serial.println("Enter WiFi SSID:");
    while (Serial.available() == 0) { delay(100); }
    String ssid = Serial.readStringUntil('\n');
    ssid.trim();
    Serial.println("SSID: " + ssid);

    Serial.println("Enter WiFi Password:");
    while (Serial.available() == 0) { delay(100); }
    String pass = Serial.readStringUntil('\n');
    pass.trim();
    // Mask password output for cleanliness, or just print it. User likely wants to see it to debug typos.
    Serial.println("Password: " + pass);

    // 2. Connect to WiFi
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
      WiFi.disconnect(true); // Clear credentials for next try
      delay(1000);
    }
  }

  // 3. Connect to Server
  Serial.print("Connecting to Server at ");
  Serial.print(SERVER_IP);
  Serial.println("...");

  if (client.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println("Connected to Server!");
  } else {
    Serial.println("Connection failed.");
    while(1);
  }
}

void loop() {
  if (!client.connected()) {
    Serial.println("Disconnected from server.");
    while(1); // Stop loop
  }

  // Read from Server -> Serial
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  // Read from Serial -> Server
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      client.println(input);
      // Local echo optional, but helpful
      // Serial.println("> Sent: " + input); 
    }
  }
}

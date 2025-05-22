#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Provide the token generation process info
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Firebase configuration
#define API_KEY "***************************************"
#define DATABASE_URL "https://###################.firebaseio.com"
#define USER_EMAIL "@@@@@@@@@@@@@@@@@@@@@@"
#define USER_PASSWORD "___________________"

// LCD configuration
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Hardware Serial for Mega communication
HardwareSerial megaSerial(1); // RX=16, TX=17

// Network credentials
const char* ssid = "*#*# *#*# @@";
const char* password = "*******************";

// NTP configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 21600, 60000); // UTC+6, update every 60s

// Firebase objects
FirebaseData fbdo;
FirebaseData streamData; // Separate object for streaming
FirebaseAuth auth;
FirebaseConfig config;

// System variables
String currentData = "{}";
unsigned long lastFirebaseUpdate = 0;
const int switchPins[9] = {13, 12, 14, 27, 26, 25, 33, 32, 35};
bool switchStates[9] = {false};

void setup() {
  Serial.begin(115200);
  megaSerial.begin(9600, SERIAL_8N1, 16, 17);

  // Initialize LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("System Starting...");
  delay(2000);

  // Initialize switches
  for (int i = 0; i < 9; i++) {
    pinMode(switchPins[i], INPUT_PULLUP);
    Serial.print("Switch ");
    Serial.print(i + 1);
    Serial.println(" Initialized");
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1000);

  // Initialize NTP
  timeClient.begin();
  lcd.clear();
  lcd.print("Syncing Time...");
  while (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500);
  }
  Serial.println("Time synchronized: " + timeClient.getFormattedTime());
  lcd.clear();
  lcd.print("Time Synced");
  delay(1000);

  // Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Verify Firebase authentication
  lcd.clear();
  lcd.print("Authenticating...");
  unsigned long authTimeout = millis();
  while (!Firebase.ready() && (millis() - authTimeout < 10000)) {
    delay(100);
  }
  if (Firebase.ready()) {
    Serial.println("Firebase authentication successful");
    lcd.clear();
    lcd.print("Firebase OK");
  } else {
    Serial.println("Firebase authentication failed: " + fbdo.errorReason());
    lcd.clear();
    lcd.print("Firebase Error");
  }
  delay(2000);

  // Set up Firebase stream with retry
  lcd.clear();
  lcd.print("Starting Stream...");
  int streamRetries = 0;
  while (!Firebase.RTDB.beginStream(&streamData, "/relayControl") && streamRetries < 3) {
    Serial.println("Stream begin error: " + streamData.errorReason());
    lcd.clear();
    lcd.print("Stream Retry " + String(streamRetries + 1));
    delay(2000);
    streamRetries++;
  }
  if (Firebase.RTDB.beginStream(&streamData, "/relayControl")) {
    Serial.println("Stream initialized at /relayControl");
    lcd.clear();
    lcd.print("Stream Ready");
    Firebase.RTDB.setStreamCallback(&streamData, streamCallback, streamTimeoutCallback);
  } else {
    Serial.println("Stream failed after retries: " + streamData.errorReason());
    lcd.clear();
    lcd.print("Stream Failed");
  }
  delay(2000);

  // Test Firebase write
  testFirebaseWrite();
}

void loop() {
  timeClient.update(); // Keep time synchronized
  readMegaData();
  checkSwitches();
  updateLCD();
  sendToFirebase();
}

void updateLCD() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 1000) return;
  lastUpdate = millis();

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, currentData);
  if (error) {
    Serial.println("LCD Error: " + String(error.c_str()));
    lcd.clear();
    lcd.print("JSON Error");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  for (int i = 1; i <= 3; i++) displaySensor(doc, i);
  lcd.setCursor(0, 1);
  for (int i = 4; i <= 6; i++) displaySensor(doc, i);
  lcd.setCursor(0, 2);
  for (int i = 7; i <= 9; i++) displaySensor(doc, i);
}

void displaySensor(JsonDocument& doc, int num) {
  String key = "m" + String(num);
  int moisture = doc[key].as<int>();
  key = "r" + String(num);
  bool relayState = doc[key].as<bool>();

  lcd.print(String(num) + ":");
  lcd.print(moisture);
  lcd.print("%");
  lcd.print(relayState ? "O " : "F ");
}

void checkSwitches() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck > 200) {
    lastCheck = millis();
    StaticJsonDocument<256> cmd;
    bool changed = false;

    for (int i = 0; i < 9; i++) {
      bool currentState = !digitalRead(switchPins[i]);
      if (currentState != switchStates[i]) {
        cmd["r" + String(i + 1)] = currentState;
        switchStates[i] = currentState;
        changed = true;
      }
    }

    if (changed) {
      String output;
      serializeJson(cmd, output);
      megaSerial.println(output);
      Serial.println("Sent to Mega from switch: " + output);
    }
  }
}

void readMegaData() {
  if (megaSerial.available()) {
    String newData = megaSerial.readStringUntil('\n');
    newData.trim();
    Serial.println("Received from Mega: " + newData);

    // Validate JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, newData);
    if (!error) {
      currentData = newData; // Only update if valid JSON
    } else {
      Serial.println("Invalid JSON from Mega: " + String(error.c_str()));
    }
  }
}

void sendToFirebase() {
  if (millis() - lastFirebaseUpdate < 10000 || currentData.isEmpty()) return;
  lastFirebaseUpdate = millis();

  if (Firebase.ready()) {
    FirebaseJson json;
    json.setJsonData(currentData);

    if (Firebase.RTDB.updateNode(&fbdo, "/sensorData", &json)) {
      Serial.println("Firebase update success");
    } else {
      Serial.println("Firebase update error: " + fbdo.errorReason());
    }
    Firebase.RTDB.setDouble(&fbdo, "/lastUpdate", millis() / 1000.0);
  } else {
    Serial.println("Firebase not ready: " + fbdo.errorReason());
  }
}

void streamCallback(FirebaseStream data) {
  Serial.println("Stream callback triggered at millis: " + String(millis()));
  String path = data.dataPath();
  Serial.println("Stream Data Path: " + path);
  Serial.println("Stream Data Type: " + String(data.dataType()));
  Serial.println("Stream Data Value: " + data.to<String>());

  if (path.startsWith("/r") && path.length() <= 3) {
    int relayNum = path.substring(2).toInt();
    if (relayNum >= 1 && relayNum <= 9) {
      if (data.dataType() == "boolean") {
        bool state = data.boolData();
        Serial.printf("Relay %d set to %s via Firebase\n", relayNum, state ? "ON" : "OFF");

        int index = relayNum - 1;
        if (switchStates[index]) {
          StaticJsonDocument<64> cmd;
          cmd["r" + String(relayNum)] = state;
          String output;
          serializeJson(cmd, output);
          megaSerial.println(output);
          Serial.println("Sent to Mega: " + output);

          // Update LCD with relay state
          lcd.clear();
          lcd.setCursor(0, 3);
          lcd.print("Relay " + String(relayNum) + ": " + (state ? "ON" : "OFF"));
        } else {
          Serial.println("Physical switch OFF - Firebase control blocked for relay " + String(relayNum));
        }
      } else {
        Serial.println("Invalid data type: Expected boolean, got " + String(data.dataType()));
      }
    } else {
      Serial.println("Invalid relay number: " + String(relayNum));
    }
  } else {
    Serial.println("Unexpected path format: " + path);
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout at millis: " + String(millis()));
    lcd.clear();
    lcd.print("Stream Timeout");
  }
}

void testFirebaseWrite() {
  if (Firebase.ready()) {
    Serial.println("Testing Firebase write...");
    if (Firebase.RTDB.setBool(&fbdo, "/relayControl/r1", true)) {
      Serial.println("Test write to /relayControl/r1: true successful");
      lcd.clear();
      lcd.print("Test Write OK");
    } else {
      Serial.println("Test write failed: " + fbdo.errorReason());
      lcd.clear();
      lcd.print("Test Write Error");
    }
  } else {
    Serial.println("Firebase not ready for test write");
  }
  delay(2000);
}

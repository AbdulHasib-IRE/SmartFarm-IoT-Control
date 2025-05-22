#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensor Configuration
const int soilPins[9] = {A9, A7, A5, A3, A1, A0, A2, A4, A6};
int moistureValues[9];

// Relay Configuration
const int relayPins[9] = {32, 34, 36, 38, 40, 42, 44, 46, 48};
bool relayStates[9] = {false};
bool overrideFlags[9] = {false};

unsigned long lastDisplayUpdate = 0;
int displayPage = 0;

void setup() {
  Serial.begin(9600);
 
  // Initialize OLED
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("System Initializing...");
  display.display();
  delay(2000);

  // Initialize relays
  for(int i=0; i<9; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
}

void loop() {
  readSensors();
  autoControl();
  handleCommands();
  sendData();
  updateDisplay();
}

void updateDisplay() {
  if(millis() - lastDisplayUpdate < 2000) return;
  lastDisplayUpdate = millis();
 
  display.clearDisplay();
  display.setCursor(0,0);
 
  for(int i=0; i<3; i++) {
    int idx = displayPage*3 + i;
    if(idx >= 9) break;
   
    display.print("Ch");
    display.print(idx+1);
    display.print(": ");
    display.print(moistureValues[idx]);
    display.print("% ");
    display.print(relayStates[idx] ? "ON " : "OFF");
    if(overrideFlags[idx]) display.print("*");
    display.println();
  }
 
  display.display();
  displayPage = (displayPage + 1) % 3;
}

void readSensors() {
  for(int i=0; i<9; i++) {
    int raw = analogRead(soilPins[i]);
    moistureValues[i] = constrain(map(raw, 1023, 400, 0, 100), 0, 100);
  }
}

void autoControl() {
  for(int i=0; i<9; i++) {
    if(!overrideFlags[i]) {
      bool newState = moistureValues[i] < 60;
      digitalWrite(relayPins[i], newState ? LOW : HIGH);
      relayStates[i] = newState;
    }
  }
}

void handleCommands() {
  while(Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
   
    StaticJsonDocument<256> doc;
    if(!deserializeJson(doc, command)) {
      for(int i=1; i<=9; i++) {
        String key = "r" + String(i);
        if(doc.containsKey(key)) {
          bool cmdState = doc[key];
          overrideFlags[i-1] = !cmdState; // Override only when turning OFF
          relayStates[i-1] = cmdState;
          digitalWrite(relayPins[i-1], cmdState ? LOW : HIGH);
        }
      }
    }
  }
}

void sendData() {
  StaticJsonDocument<512> doc;
  for(int i=0; i<9; i++) {
    doc["m" + String(i+1)] = moistureValues[i];
    doc["r" + String(i+1)] = relayStates[i];
  }
 
  serializeJson(doc, Serial);
  Serial.println();
}

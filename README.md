# SmartFarm-IoT-Control 🌱  
**Automated Agricultural Monitoring & Irrigation System**  

---

## Features  
- **9-Channel Soil Monitoring**: Analog sensors on Arduino Mega pro mini pins `A9, A7, A5, A3, A1, A0, A2, A4, A6`.  
- **Relay Control**:  
  - **Auto Mode**: Activates irrigation when moisture < 60%.  
  - **Physical Override**: 9 toggle switches on ESP32 GPIOs `13,12,14,27,26,25,33,32,35`.  
  - **Firebase Remote Control**: Real-time commands via Firebase Realtime Database.  
- **Displays**:  
  - **OLED (Mega)**: Shows 3 zones per page with moisture % and relay status.  
  - **20x4 LCD (ESP32)**: Displays all 9 zones simultaneously.  
- **Data Sync**: Firebase integration for cloud storage and remote access.  

---

## Hardware Setup  

### Arduino Mega Pro 2560 Mini
| Component       | Pins                              |  
|-----------------|-----------------------------------|  
| Soil Sensors    | `A9, A7, A5, A3, A1, A0, A2, A4, A6` |  
| Relays          | `32,34,36,38,40,42,44,46,48`     |  
| OLED Display    | I2C (Default SDA/SCL)            |  

### ESP32 Dev Board  
| Component       | GPIO Pins                         |  
|-----------------|-----------------------------------|  
| Mega Serial     | RX=`16`, TX=`17`                  |  
| Toggle Switches | `13,12,14,27,26,25,33,32,35`     |  
| 20x4 LCD        | SDA=`21`, SCL=`22` (I2C@`0x27`)  |  

---

## Firebase Configuration  

1. **Create Project** at [Firebase Console](https://firebase.google.com).  
2. **Enable Services**:  
   - Realtime Database (Test Mode)  
   - Email/Password Authentication  
3. **Update ESP32 Code**:  
   ```cpp  
   // Replace these lines in ESP32-Code.ino  
   #define API_KEY "YOUR_FIREBASE_API_KEY"       // Line 13  
   #define DATABASE_URL "YOUR_DATABASE_URL"      // Line 14  
   #define USER_EMAIL "YOUR_EMAIL"               // Line 15  
   #define USER_PASSWORD "YOUR_PASSWORD"         // Line 16  
   const char* ssid = "YOUR_WIFI_SSID";          // Line 32  
   const char* password = "YOUR_WIFI_PASSWORD";  // Line 33  



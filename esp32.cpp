#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "time.h" // ไลบรารีสำหรับดึงเวลา

// --- 1. ตั้งค่า WiFi / Firebase / Time ---
#define API_KEY "AIzaSyAMvydTgXJLog3_H5LAw1o8xnhHP2aTAHQ"
#define DATABASE_URL "https://embeded-finalproject-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define SSID "Meee"
#define PASSWORD "11111111"

// ตั้งค่าเวลา (NTP Server)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // GMT+7 (Thailand)
const int   daylightOffset_sec = 0;
unsigned long lastFirebaseCheck = 0;
const unsigned long checkInterval = 2000;

// --- ตั้งค่า UART (Serial1) ---
#define RX_PIN 18
#define TX_PIN 17

HardwareSerial mySTM32Serial(2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  mySTM32Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  // เชื่อมต่อ WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n[WiFi] Connected!");

  // --- 2. เริ่มต้นดึงเวลาจาก Internet ---
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Waiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n[Time] Synced!");

  // ตั้งค่า Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("[Firebase] SignUp OK");
  } else {
    Serial.printf("[Firebase] SignUp Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (mySTM32Serial.available()) {
    String inputData = mySTM32Serial.readStringUntil('\n');
    inputData.trim(); 
    // Serial.println(inputData);
    if (inputData.length() > 0) {
      Serial.print("[UART] Received: ");
      Serial.println(inputData);
      
      processInput(inputData);
    }
  }
  if (millis() - lastFirebaseCheck > checkInterval) {
    lastFirebaseCheck = millis();
    checkManualFeed();
  }
}
void checkManualFeed() {
  // Only run if Firebase is ready
  if (Firebase.ready()) {
    
    // 1. Read the value from /manual/value
    if (Firebase.RTDB.getBool(&fbdo, "/manual/value")) {
      
      if (fbdo.dataType() == "boolean") {
        bool shouldFeed = fbdo.boolData();

        if (shouldFeed == true) {
          Serial.println("\n[Manual] Feed command received from App!");

          // 2. Send 'f' to STM32
          // Note: We use print('f') to send just the character. 
          // If your STM32 expects a newline, change to mySTM32Serial.println("f");
          mySTM32Serial.print("f"); 

          // 3. Reset the value to false immediately
          if (Firebase.RTDB.setBool(&fbdo, "/manual/value", false)) {
            Serial.println("[Manual] Flag reset to false.");
          } else {
            Serial.print("[Error] Could not reset flag: ");
            Serial.println(fbdo.errorReason());
          }
        }
      }
    } else {
    }
  }
}
// ฟังก์ชันแยกประเภทข้อมูล
void processInput(String data) {
  
  // --- Case 1: Manual Log (starts with 'm') ---
  if (data.startsWith("m") || data.startsWith("a")) {
    
    // 1. ดึงเวลาปัจจุบันมาจัดรูปแบบ
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("[Error] Failed to obtain time");
      return;
    }
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    String timestampStr = String(timeStringBuff);

    // 2. สร้าง JSON
    FirebaseJson json;
    if (data.startsWith("m") ){
      json.set("type", "manual");
    }
    else{
      json.set("type", "auto");
    }
    
    json.set("timestamp", timestampStr);
    Serial.println("[Firebase] Saving to Circular Log (Max 4)...");
    
    // Step A: Get the current slot index from Firebase
    int currentSlot = 0;
    if (Firebase.RTDB.getInt(&fbdo, "/logs_config/next_slot")) {
      currentSlot = fbdo.intData();
    } else {
      // If it doesn't exist yet (first run), start at 0
      currentSlot = 0; 
    }

    // Step B: Write data to the calculated slot (e.g., /logs/slot_0)
    String path = "/logs/slot_" + String(currentSlot);
    if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
       Serial.printf("[Success] Log saved to slot_%d\n", currentSlot);
    } else {
       Serial.println(fbdo.errorReason());
    }

    // Step C: Calculate next slot (0 -> 1 -> 2 -> 3 -> 0)
    int nextSlot = (currentSlot + 1) % 4; // Modulo operator loops it back to 0

    // Step D: Update the pointer in Firebase for next time
    Firebase.RTDB.setInt(&fbdo, "/logs_config/next_slot", nextSlot);
    // ---------------------------------------------------------
  }
  
  // --- Case 2: Data Update (starts with "d:") ---
  else if (data.startsWith("d:")) {
    String rawValues = data.substring(2); 

    int firstComma = rawValues.indexOf(',');
    int secondComma = rawValues.indexOf(',', firstComma + 1);

    if (firstComma == -1 || secondComma == -1) return;

    String refillStr = rawValues.substring(0, firstComma);
    String weightStr = rawValues.substring(firstComma + 1, secondComma);
    String waterStr = rawValues.substring(secondComma + 1);

    FirebaseJson json;
    json.set("refill_left", refillStr.toInt());
    json.set("weight", weightStr.toInt());
    json.set("water_level", waterStr.toInt());

    Serial.println("[Firebase] Updating Sensor Data...");
    if (Firebase.RTDB.setJSON(&fbdo, "/data", &json)) {
       Serial.println("[Success] Data updated!");
    } else {
       Serial.println(fbdo.errorReason());
    }
  }else {
    Serial.println("Wrong UART format!");
  }
}
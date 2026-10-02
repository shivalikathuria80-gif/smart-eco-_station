/*
 * ============================================================================
 *  SMART ECO STATION  -  MAIN CONTROLLER (ESP32 DevKit)
 * ============================================================================
 *  Flow:
 *    1. User taps RFID card (RC522)          -> checked against Firebase /users
 *    2. Valid card -> solenoid lock opens    -> user drops waste on the flap
 *    3. Main ESP32 asks ESP32-CAM (UART)     -> "WET" or "DRY"
 *    4. Servo tilts flap into the right bin  -> user gets reward points
 *    5. Two ultrasonic sensors measure fill  -> LCD + Firebase /bins (portal)
 *
 *  Firebase paths (same as the website "smart eco station.html"):
 *    /users/{UID}          { name, points }
 *    /bins/{STATION_ID}    { zone, columns: { wet, dry }, updated }
 *    /logs/{pushId}        { type: scan|reward|alert, text, time }
 *
 *  Libraries (Arduino Library Manager):
 *    - MFRC522            (GithubCommunity)
 *    - ESP32Servo         (Kevin Harrington)
 *    - LiquidCrystal I2C  (Frank de Brabander)
 *  Board: "ESP32 Dev Module"  (esp32 core by Espressif, v2.x or v3.x)
 * ============================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ---------------------------------------------------------------- SETTINGS --
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Realtime Database URL from the website's firebaseConfig (no trailing slash)
const char* FIREBASE_URL  = "https://smart-eco-station-default-rtdb.europe-west1.firebasedatabase.app";
// Leave "" if your DB rules allow writes; otherwise put a Database Secret here
const char* FIREBASE_AUTH = "";

const char* STATION_ID    = "STATION-01";     // shows as the bin card title
const char* STATION_ZONE  = "School Block A"; // shows under the bin card title

const int   POINTS_PER_ITEM = 10;
const float MIN_CONFIDENCE  = 0.60;  // below this the item is not sorted / not rewarded

// Bin geometry (cm) - measure your own dustbins!
const float BIN_DEPTH_CM = 40.0;  // sensor to bottom of empty bin
const float FULL_GAP_CM  = 5.0;   // sensor to waste when we call it 100%
const int   FULL_ALERT_PERCENT = 90;

// Servo angles for the diverter flap
const int SERVO_CENTER = 90;
const int SERVO_WET    = 30;
const int SERVO_DRY    = 150;

// Timings (ms)
const unsigned long LOCK_OPEN_TIME   = 5000;   // time lid stays unlocked
const unsigned long ITEM_SETTLE_TIME = 1500;   // wait for item to rest on flap
const unsigned long CAM_TIMEOUT      = 8000;
const unsigned long BIN_UPLOAD_EVERY = 15000;
const unsigned long WIFI_RETRY_EVERY = 10000;

// ------------------------------------------------------------------- PINS --
// RC522 (SPI): SCK 18, MISO 19, MOSI 23
#define RFID_SS_PIN    5
#define RFID_RST_PIN   27
// I2C LCD: SDA 21, SCL 22
#define LCD_ADDR       0x27      // try 0x3F if screen stays blank
#define SOLENOID_PIN   26        // -> relay / MOSFET IN (NOT directly to lock!)
#define SERVO_PIN      13
#define BUZZER_PIN     4
// Ultrasonic HC-SR04 (ECHO is 5V -> use a 1k/2k voltage divider)
#define WET_TRIG_PIN   32
#define WET_ECHO_PIN   33
#define DRY_TRIG_PIN   25
#define DRY_ECHO_PIN   14
// UART to ESP32-CAM
#define CAM_RX_PIN     16        // <- CAM U0T (GPIO1)
#define CAM_TX_PIN     17        // -> CAM U0R (GPIO3)

#define SOLENOID_ON    HIGH      // flip if your relay module is active-LOW
#define SOLENOID_OFF   LOW

// ---------------------------------------------------------------- OBJECTS --
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
Servo flap;
HardwareSerial CamSerial(2);

int wetLevel = 0, dryLevel = 0;
bool wetAlertSent = false, dryAlertSent = false;
unsigned long lastBinUpload = 0;
unsigned long lastWifiRetry = 0;

// ================================================================ HELPERS ==
void beep(int times, int ms = 80) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(ms);
    digitalWrite(BUZZER_PIN, LOW);  delay(ms);
  }
}

void lcdShow(const String& l1, const String& l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1.substring(0, 16));
  lcd.setCursor(0, 1); lcd.print(l2.substring(0, 16));
}

void lcdIdle() {
  lcdShow("Wet:" + String(wetLevel) + "% Dry:" + String(dryLevel) + "%",
          (wetLevel >= FULL_ALERT_PERCENT || dryLevel >= FULL_ALERT_PERCENT)
            ? "!! BIN FULL !!" : "Tap your card");
}

String timeNow() {
  struct tm t;
  if (!getLocalTime(&t, 100)) return "--:--:--";
  char buf[12];
  strftime(buf, sizeof(buf), "%I:%M:%S %p", &t);
  return String(buf);
}

// UID bytes -> "A3 5F 8C 12" (same format the website registers)
String readUid() {
  String uid;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (i) uid += " ";
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

String urlEncode(const String& s) {
  String out;
  for (char c : s) out += (c == ' ') ? String("%20") : String(c);
  return out;
}

String jsonEscape(const String& s) {
  String out;
  for (char c : s) { if (c == '"' || c == '\\') out += '\\'; out += c; }
  return out;
}

// ============================================================== FIREBASE ==
// method: GET / PUT / PATCH / POST. Returns response body, "" on failure.
String firebase(const char* method, const String& path, const String& body = "") {
  if (WiFi.status() != WL_CONNECTED) return "";
  WiFiClientSecure client;
  client.setInsecure();                   // skip cert check (fine for a school project)
  HTTPClient http;
  String url = String(FIREBASE_URL) + "/" + path + ".json";
  if (strlen(FIREBASE_AUTH)) url += "?auth=" + String(FIREBASE_AUTH);
  if (!http.begin(client, url)) return "";
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest(method, body);
  String resp = (code > 0) ? http.getString() : "";
  http.end();
  if (code != 200) {
    Serial.printf("[Firebase] %s %s -> %d %s\n", method, path.c_str(), code, resp.c_str());
    return "";
  }
  return resp;
}

void pushLog(const char* type, const String& text) {
  firebase("POST", "logs",
           "{\"type\":\"" + String(type) + "\",\"text\":\"" + jsonEscape(text) +
           "\",\"time\":\"" + timeNow() + "\"}");
}

// Returns the user's name, or "" if the card isn't registered
String getUserName(const String& uid) {
  String r = firebase("GET", "users/" + urlEncode(uid) + "/name");
  r.trim();
  if (r.length() < 2 || r == "null") return "";
  return r.substring(1, r.length() - 1);  // strip JSON quotes
}

// Adds points atomically on the Firebase server (same as the website's
// transaction), so a failed read can never overwrite a balance with 0.
// Returns the new total, or -1 on failure.
long addPoints(const String& uid, int delta) {
  String path = "users/" + urlEncode(uid) + "/points";
  String r = firebase("PUT", path, "{\".sv\":{\"increment\":" + String(delta) + "}}");
  r.trim();
  if (r == "") return -1;
  return r.toInt();
}

void uploadBinLevels() {
  firebase("PATCH", "bins/" + String(STATION_ID),
           "{\"zone\":\"" + jsonEscape(STATION_ZONE) + "\","
           "\"columns\":{\"wet\":" + String(wetLevel) + ",\"dry\":" + String(dryLevel) + "},"
           "\"updated\":\"" + timeNow() + "\"}");
}

// ============================================================ ULTRASONIC ==
float readDistanceCm(int trig, int echo) {
  float sum = 0; int good = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(trig, LOW);  delayMicroseconds(2);
    digitalWrite(trig, HIGH); delayMicroseconds(10);
    digitalWrite(trig, LOW);
    long us = pulseIn(echo, HIGH, 30000);   // 30 ms timeout (~5 m)
    if (us > 0) { sum += us * 0.0343 / 2.0; good++; }
    delay(30);
  }
  return good ? sum / good : BIN_DEPTH_CM;   // no echo -> treat as empty
}

int toPercent(float distance) {
  float pct = (BIN_DEPTH_CM - distance) / (BIN_DEPTH_CM - FULL_GAP_CM) * 100.0;
  return constrain((int)pct, 0, 100);
}

void updateBinLevels() {
  wetLevel = toPercent(readDistanceCm(WET_TRIG_PIN, WET_ECHO_PIN));
  dryLevel = toPercent(readDistanceCm(DRY_TRIG_PIN, DRY_ECHO_PIN));
  Serial.printf("[Bins] wet=%d%% dry=%d%%\n", wetLevel, dryLevel);

  // One alert per overflow; reset once the bin is emptied
  if (wetLevel >= FULL_ALERT_PERCENT && !wetAlertSent) {
    pushLog("alert", String(STATION_ID) + " - WET bin is " + wetLevel + "% full. Please empty.");
    wetAlertSent = true;
  } else if (wetLevel < FULL_ALERT_PERCENT - 20) wetAlertSent = false;

  if (dryLevel >= FULL_ALERT_PERCENT && !dryAlertSent) {
    pushLog("alert", String(STATION_ID) + " - DRY bin is " + dryLevel + "% full. Please empty.");
    dryAlertSent = true;
  } else if (dryLevel < FULL_ALERT_PERCENT - 20) dryAlertSent = false;
}

// ============================================================== ESP32-CAM ==
// Sends "CAPTURE", expects "RESULT:WET:0.93", "RESULT:DRY:0.88" or
// "RESULT:NONE:0.97" (nothing on the flap).
// Returns "WET", "DRY", "NONE" (empty / not sure) or "" on timeout/error.
String classifyWaste() {
  while (CamSerial.available()) CamSerial.read();   // flush old data
  CamSerial.println("CAPTURE");
  unsigned long start = millis();
  String line;
  while (millis() - start < CAM_TIMEOUT) {
    if (!CamSerial.available()) { delay(5); continue; }
    char c = CamSerial.read();
    if (c == '\n') {
      line.trim();
      Serial.println("[CAM] " + line);
      if (line.startsWith("RESULT:")) {
        int c2 = line.indexOf(':', 7);
        String label = line.substring(7, c2 < 0 ? line.length() : c2);
        float conf = c2 < 0 ? 1.0 : line.substring(c2 + 1).toFloat();
        if ((label == "WET" || label == "DRY") && conf >= MIN_CONFIDENCE) return label;
        return "NONE";
      }
      if (line.startsWith("ERROR:")) return "";
      line = "";                                    // ignore debug lines
    } else line += c;
  }
  return "";
}

void sortItem(bool wet) {
  flap.write(wet ? SERVO_WET : SERVO_DRY);
  delay(1200);
  flap.write(SERVO_CENTER);
  delay(500);
}

// ================================================================= SETUP ==
void setup() {
  Serial.begin(115200);
  CamSerial.begin(115200, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);

  pinMode(SOLENOID_PIN, OUTPUT); digitalWrite(SOLENOID_PIN, SOLENOID_OFF);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(WET_TRIG_PIN, OUTPUT); pinMode(WET_ECHO_PIN, INPUT);
  pinMode(DRY_TRIG_PIN, OUTPUT); pinMode(DRY_ECHO_PIN, INPUT);

  lcd.init(); lcd.backlight();
  lcdShow("Smart Eco", "Station booting");

  flap.setPeriodHertz(50);
  flap.attach(SERVO_PIN, 500, 2400);
  flap.write(SERVO_CENTER);

  SPI.begin();
  rfid.PCD_Init();

  lcdShow("Connecting WiFi", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK: " + WiFi.localIP().toString());
    configTime(19800, 0, "pool.ntp.org");            // IST (UTC+5:30)
    lcdShow("WiFi connected", WiFi.localIP().toString());
  } else {
    lcdShow("WiFi FAILED", "Offline mode");
  }
  delay(1500);

  // Boot check: prove the database link works before users arrive
  updateBinLevels();
  String ok = "";
  if (WiFi.status() == WL_CONNECTED) {
    uploadBinLevels();
    ok = firebase("GET", "bins/" + String(STATION_ID) + "/zone");
  }
  Serial.println(ok.length() ? "[Firebase] link OK" : "[Firebase] link FAILED - check URL / rules / FIREBASE_AUTH");
  lcdShow(ok.length() ? "Firebase OK" : "Firebase FAILED", ok.length() ? STATION_ID : "Check rules/auth");
  if (!ok.length()) beep(3, 300);
  delay(1500);
  lastBinUpload = millis();
  lcdIdle();
}

// ================================================================== LOOP ==
void loop() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiRetry > WIFI_RETRY_EVERY) {
    WiFi.reconnect();                                // retry every 10 s, not every loop
    lastWifiRetry = millis();
  }

  // Periodic fill-level report (LCD + website)
  if (millis() - lastBinUpload > BIN_UPLOAD_EVERY) {
    updateBinLevels();
    uploadBinLevels();
    lastBinUpload = millis();
    lcdIdle();
  }

  // ---- 1. Wait for a card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  String uid = readUid();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println("[RFID] " + uid);

  lcdShow("Card detected", "Checking...");
  String name = getUserName(uid);

  if (name == "") {
    lcdShow("ACCESS DENIED", uid);
    beep(3, 150);
    pushLog("scan", "Unknown card " + uid + " denied at " + STATION_ID);
    delay(2500);
    lcdIdle();
    return;
  }

  // ---- 2. Unlock
  lcdShow("Hi " + name, "Lid unlocked!");
  beep(1);
  pushLog("scan", name + " (" + uid + ") scanned at " + STATION_ID);
  digitalWrite(SOLENOID_PIN, SOLENOID_ON);
  delay(LOCK_OPEN_TIME);
  digitalWrite(SOLENOID_PIN, SOLENOID_OFF);   // re-lock (never keep solenoid on long)

  // ---- 3. Classify
  lcdShow("Scanning waste", "Please wait...");
  delay(ITEM_SETTLE_TIME);
  String type = classifyWaste();
  if (type == "") {
    lcdShow("Camera error", "Try again");
    beep(2, 200);
    delay(2000);
    lcdIdle();
    return;
  }
  if (type == "NONE") {                            // empty flap or unsure -> no points
    lcdShow("No waste seen", "No points given");
    beep(2, 200);
    pushLog("scan", name + " (" + uid + ") - no item detected, no points");
    delay(2500);
    lcdIdle();
    return;
  }
  bool wet = (type == "WET");

  // ---- 4. Check target bin isn't full, then sort
  updateBinLevels();
  if ((wet ? wetLevel : dryLevel) >= 100) {
    lcdShow(wet ? "WET bin FULL" : "DRY bin FULL", "Item not sorted");
    beep(3, 200);
    pushLog("alert", String(STATION_ID) + " - " + type + " bin full, item rejected");
    delay(3000);
    lcdIdle();
    return;
  }

  lcdShow("Detected: " + type, wet ? "-> Wet bin" : "-> Dry bin");
  sortItem(wet);

  // ---- 5. Reward
  long total = addPoints(uid, POINTS_PER_ITEM);
  if (total >= 0) {
    lcdShow("+" + String(POINTS_PER_ITEM) + " points!", "Total: " + String(total));
    pushLog("reward", name + " earned +" + String(POINTS_PER_ITEM) + " pts (" + type + " waste)");
  } else {
    lcdShow("Sorted: " + type, "Points sync fail");
  }
  beep(2);

  // ---- 6. Report new fill levels immediately
  updateBinLevels();
  uploadBinLevels();
  lastBinUpload = millis();
  delay(2500);
  lcdIdle();
}

/*
 * ============================================================================
 *  SMART ECO STATION  -  ESP32-CAM (Teachable Machine server version)
 * ============================================================================
 *  Alternative to EcoStation_Cam: instead of running the model on the CAM,
 *  the photo is POSTed to ClassifierServer/app.py (Teachable Machine model on
 *  a PC / Raspberry Pi on the same WiFi).
 *
 *  UART protocol with the main ESP32 is identical, so EcoStation_Main needs
 *  no changes:  receives "CAPTURE"  ->  replies "RESULT:WET:0.93" / "RESULT:DRY:0.88"
 *
 *  Board: "AI Thinker ESP32-CAM", PSRAM enabled.
 *  Unplug the GPIO1/GPIO3 wires to the main ESP32 while flashing.
 * ============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// Printed by app.py when it starts, e.g. "http://192.168.1.20:5000/classify"
const char* SERVER_URL    = "http://192.168.1.20:5000/classify";

// ---------------- AI-Thinker pin map
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22
#define FLASH_LED_PIN    4

bool initCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM; c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;        // 640x480, server resizes to 224x224
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  return esp_camera_init(&c) == ESP_OK;
}

// Pulls a string value out of the server's small JSON reply
String jsonString(const String& json, const String& key) {
  int k = json.indexOf("\"" + key + "\"");
  if (k < 0) return "";
  int colon = json.indexOf(':', k);
  int q1 = json.indexOf('"', colon + 1);
  int nullPos = json.indexOf("null", colon + 1);
  if (q1 < 0 || (nullPos >= 0 && nullPos < q1)) return "";
  return json.substring(q1 + 1, json.indexOf('"', q1 + 1));
}

float jsonFloat(const String& json, const String& key) {
  int k = json.indexOf("\"" + key + "\"");
  if (k < 0) return 0;
  return json.substring(json.indexOf(':', k) + 1).toFloat();
}

void classify() {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("ERROR:WIFI"); return; }

  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(150);
  camera_fb_t* fb = esp_camera_fb_get();       // discard stale frame
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  digitalWrite(FLASH_LED_PIN, LOW);
  if (!fb) { Serial.println("ERROR:CAPTURE"); return; }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.setTimeout(6000);                        // main board waits up to 8 s
  http.addHeader("Content-Type", "image/jpeg");
  int code = http.POST(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (code != 200) {
    Serial.printf("ERROR:SERVER_%d\n", code);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();
  Serial.println("  server: " + body);

  // app.py returns {"label": ..., "confidence": ..., "stream": "WET"|"DRY"|"NONE"|null}
  String stream = jsonString(body, "stream");
  float conf = jsonFloat(body, "confidence");
  if (stream == "WET" || stream == "DRY" || stream == "NONE") Serial.printf("RESULT:%s:%.2f\n", stream.c_str(), conf);
  else Serial.println("ERROR:UNMAPPED_LABEL");
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  if (!initCamera()) {
    Serial.println("ERROR:CAMERA_INIT");
    while (true) delay(1000);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  Serial.println(WiFi.status() == WL_CONNECTED ? "CAM READY " + WiFi.localIP().toString()
                                               : String("ERROR:WIFI"));
}

unsigned long lastWifiRetry = 0;

void loop() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiRetry > 10000) {
    WiFi.reconnect();
    lastWifiRetry = millis();
  }
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "CAPTURE") classify();
  }
}

/*
 * ============================================================================
 *  SMART ECO STATION  -  ESP32-CAM WASTE CLASSIFIER (AI-Thinker board)
 * ============================================================================
 *  Waits for "CAPTURE" on Serial (from main ESP32), takes a photo, runs the
 *  Edge Impulse model and replies:   RESULT:WET:0.93   or   RESULT:DRY:0.88
 *
 *  Setup:
 *    1. Train an image-classification model on Edge Impulse with exactly two
 *       labels: "wet" and "dry" (photos taken with THIS camera on the flap).
 *    2. Deployment -> Arduino library -> Sketch > Include Library > Add .ZIP.
 *    3. Change the #include below to the header name of your library.
 *  Board: "AI Thinker ESP32-CAM", PSRAM enabled.
 *  NOTE: GPIO1/GPIO3 are also used for uploading - unplug the wires to the
 *        main ESP32 while flashing.
 * ============================================================================
 */

#include <Wet_Dry_Waste_inferencing.h>        // <-- your Edge Impulse library
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

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

#define RAW_W 320
#define RAW_H 240

static uint8_t* snapshot = nullptr;

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
  c.frame_size = FRAMESIZE_QVGA;       // 320x240
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  return esp_camera_init(&c) == ESP_OK;
}

// Edge Impulse pulls pixels through this callback (packed 0xRRGGBB floats)
static int getData(size_t offset, size_t length, float* out) {
  size_t px = offset * 3;
  for (size_t i = 0; i < length; i++) {
    out[i] = (snapshot[px + 2] << 16) + (snapshot[px + 1] << 8) + snapshot[px];
    px += 3;
  }
  return 0;
}

bool captureImage() {
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(150);
  camera_fb_t* fb = esp_camera_fb_get();       // discard stale frame
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  digitalWrite(FLASH_LED_PIN, LOW);
  if (!fb) return false;

  bool ok = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot);
  esp_camera_fb_return(fb);
  if (!ok) return false;

  ei::image::processing::crop_and_interpolate_rgb888(
      snapshot, RAW_W, RAW_H,
      snapshot, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  return true;
}

void classify() {
  if (!captureImage()) { Serial.println("ERROR:CAPTURE"); return; }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data = &getData;

  ei_impulse_result_t result = {};
  if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) {
    Serial.println("ERROR:MODEL");
    return;
  }

  float wet = 0, dry = 0;
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    String label = String(result.classification[i].label);
    label.toLowerCase();
    Serial.printf("  %s: %.2f\n", label.c_str(), result.classification[i].value);
    if (label == "wet") wet = result.classification[i].value;
    if (label == "dry") dry = result.classification[i].value;
  }
  // Main board only reacts to the line starting with "RESULT:"
  if (wet > dry) Serial.printf("RESULT:WET:%.2f\n", wet);
  else           Serial.printf("RESULT:DRY:%.2f\n", dry);
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  snapshot = (uint8_t*)ps_malloc(RAW_W * RAW_H * 3);
  if (!snapshot || !initCamera()) {
    Serial.println("ERROR:CAMERA_INIT");
    while (true) delay(1000);
  }
  Serial.println("CAM READY");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "CAPTURE") classify();
  }
}

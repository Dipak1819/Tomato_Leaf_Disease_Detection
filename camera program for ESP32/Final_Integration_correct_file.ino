#include "esp_camera.h"

#define CAMERA_MODEL_WROVER_KIT
#include "camera_pins.h"

const int triggerPin = 12;  // GPIO pin to receive trigger from FRDM-K64F
const int sprayActivationPin = 13;  // GPIO pin to send activation signal to FRDM-K64F

void setup() {
  Serial.begin(115200);
  pinMode(triggerPin, INPUT);
  pinMode(sprayActivationPin, OUTPUT);
  digitalWrite(sprayActivationPin, LOW); // Ensure spray signal is inactive initially

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error: %d\n", err);
    return;
  }

  Serial.println("Camera initialized successfully");
  Serial.println("ESP32 ready - waiting for commands");
}

void loop() {
  // Listen for trigger signal from FRDM-K64F
  if (digitalRead(triggerPin) == HIGH) {
    Serial.println("Trigger detected from FRDM board");
    captureAndSendImage();
    delay(500); // Debounce delay
  }

  // Listen for signal from Python
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "D" || command == "UNHEALTHY") { // Accept both formats
      Serial.println("Unhealthy leaf detected, activating spray");
      activateSpraySystem();
    }
    else if (command == "H" || command == "HEALTHY") {
      Serial.println("Healthy leaf detected, no action needed");
    }
  }
  
  // Small delay to prevent tight loop
  delay(50);
}

void captureAndSendImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.println("START_IMAGE");
  Serial.print("SIZE:");
  Serial.println(fb->len);
  
  Serial.write(fb->buf, fb->len);
  
  Serial.println("END_IMAGE");
  
  esp_camera_fb_return(fb);
  
  Serial.println("Image sent successfully");
}

void activateSpraySystem() {
   // Set HIGH to activate signal to FRDM-K64F
   digitalWrite(sprayActivationPin, HIGH);
   Serial.println("SPRAY_SIGNAL_SENT");  // Acknowledgment for Python program
   
   // Keep spray signal active for 1 second
   delay(1000);
   
   // Set back to LOW when done
   digitalWrite(sprayActivationPin, LOW);
   Serial.println("Spray system deactivated");
}
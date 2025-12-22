#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// --- CONFIGURATION ---
#define WIFI_CHANNEL 13    // The "Magic Frequency" (Clean in Europe)
// ---------------------

typedef struct struct_message {
  char msg[32];
  unsigned long timestamp;
} struct_message;

struct_message incomingData;

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  
  // We only print the magic word. Python handles the rest.
  Serial.println("TOGGLE");
}

void setup() {
  Serial.begin(115200);
  
  // 1. Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 

  // 2. Force Channel 13
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // 3. Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    // If it fails, we just sit silent. 
    // Restarting here can cause boot loops if power is shaky.
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  
  // Print Ready message (Python looks for this to confirm connection)
  Serial.println("SERIAL_DONGLE_READY");
}

void loop() {
  // Dongle sleeps 99% of the time, waiting for interrupts
  delay(100);
}
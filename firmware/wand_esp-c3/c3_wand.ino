#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

// --- CONFIGURATION ---
#define WIFI_CHANNEL 13    // Must match Receiver
#define SDA_PIN 8
#define SCL_PIN 9
// ---------------------

// Receiver Address (Your specific Dongle MAC)
uint8_t receiverMAC[] = {0x84, 0x1F, 0xE8, 0x17, 0x62, 0xD4};

// MPU6050 Registers
#define MPU6050_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B

typedef struct {
  char msg[32];
  unsigned long timestamp;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Sensitivity: Higher = Less Sensitive. 
// 15000 is a hard wave. 5000 is a gentle nudge.
const int16_t MOTION_THRESHOLD = 12000; 

void setup() {
  // 1. Init Sensors
  Wire.begin(SDA_PIN, SCL_PIN);
  setupMPU();

  // 2. Check for Motion
  // We read the accelerometer. If the forces are high (a wave), we send.
  if (checkMotion()) {
    sendBurstMessage();
  }

  // 3. Go to Sleep
  // We sleep for 100ms, then wake up to check motion again.
  // This feels "instant" to the user but saves battery.
  esp_sleep_enable_timer_wakeup(100 * 1000ULL); 
  esp_deep_sleep_start();
}

void loop() {}

void setupMPU() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0); // Wake up MPU
  Wire.endTransmission();
}

bool checkMotion() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 6, true);

  int16_t AcX = Wire.read() << 8 | Wire.read();
  int16_t AcY = Wire.read() << 8 | Wire.read();
  int16_t AcZ = Wire.read() << 8 | Wire.read();

  // Simple "Total Force" calculation (removing gravity approx)
  // If the total acceleration changes significantly, it's a wave.
  // Note: This is a simplified "jerk" check for deep sleep efficiency.
  long totalForce = abs(AcX) + abs(AcY) + abs(AcZ);
  
  // Gravity is usually around 16000-17000 on these sensors.
  // If we deviate significantly or spike, we trigger.
  // We check if X or Y (side to side wave) is high.
  if (abs(AcX) > MOTION_THRESHOLD || abs(AcY) > MOTION_THRESHOLD) {
    return true;
  }
  return false;
}

void sendBurstMessage() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // High Power & Clean Channel
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() == ESP_OK) {
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    strcpy(myData.msg, "TOGGLE");
    myData.timestamp = millis();

    // BURST FIRE: Send 4 times to ensure delivery
    for(int i=0; i<4; i++) {
      esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));
      delay(5); 
    }
  }
  WiFi.mode(WIFI_OFF);
  
  // Cooldown delay so one wave doesn't trigger 50 times
  delay(500); 
}
#include <Arduino.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

// --- CONFIGURATION ---
// Receiver Address (Your Dongle MAC)
uint8_t receiverMAC[] = {0x84, 0x1F, 0xE8, 0x17, 0x62, 0xD4};
#define WIFI_CHANNEL 13 

// Hardware Pins (ESP32-C3 Supermini)
#define INT_PIN GPIO_NUM_4  
#define SDA_PIN 8
#define SCL_PIN 9
#define MPU_ADDR 0x68

// Sensitivity (Higher = Less Sensitive)
// 20 = Very sensitive, 40 = Standard, 60 = Hard shake
#define MOTION_SENSITIVITY 40 

// --- MPU REGISTERS ---
#define INT_PIN_CFG        0x37
#define INT_ENABLE         0x38
#define INT_STATUS         0x3A
#define ACCEL_CONFIG       0x1C
#define MOT_THR            0x1F  
#define MOT_DUR            0x20  
#define PWR_MGMT_1         0x6B

// --- ESP-NOW STRUCTURE ---
typedef struct {
  char msg[32];
  unsigned long timestamp;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// RTC Memory - These variables survive Deep Sleep!
RTC_DATA_ATTR int bootCount = 0;

void sendBurstMessage();
void configureMPU_WakeOnMotion();

void setup() {
  // 1. Init I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // 2. CLEAR THE INTERRUPT (Crucial Step)
  // We must read the status register to reset the INT pin LOW.
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(INT_STATUS);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1, (bool)true);
  Wire.read(); 

  // 3. Send the Message (Only if this is not the first plugin)
  // bootCount > 0 means we woke up from sleep, not fresh power insertion.
  if (bootCount > 0) {
    sendBurstMessage();
  }
  bootCount++;

  // 4. Re-Configure MPU for the NEXT sleep
  configureMPU_WakeOnMotion();

  // 5. Enable Wakeup & Sleep
  // We use the C3 specific GPIO wakeup function
  esp_deep_sleep_enable_gpio_wakeup(1ULL << INT_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
}

void loop() {}

void sendBurstMessage() {
  // Init WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Power & Channel Setup
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // Init ESP-NOW
  if (esp_now_init() == ESP_OK) {
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    strcpy(myData.msg, "TOGGLE");
    myData.timestamp = millis();

    // --- BURST LOGIC ---
    // Send 3 times. If Packet 1 is lost, Packet 2 or 3 will hit.
    // Server-side debounce will ignore the extras.
    for(int i=0; i<3; i++) {
      esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));
      delay(2); // Tiny delay to let the radio queue clear
    }
  }
  
  // Clean shutdown of WiFi before sleep
  WiFi.mode(WIFI_OFF);
}

void configureMPU_WakeOnMotion() {
  // Reset
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x80); 
  Wire.endTransmission();
  delay(50); 

  // Wake up
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();

  // High Pass Filter (5Hz)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_CONFIG);
  Wire.write(0x01); 
  Wire.endTransmission();

  // Interrupt Config (Latch + Clear on Read)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(INT_PIN_CFG);
  Wire.write(0x30); 
  Wire.endTransmission();

  // Enable Motion Interrupt
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(INT_ENABLE);
  Wire.write(0x40); 
  Wire.endTransmission();

  // Threshold
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MOT_THR);
  Wire.write(MOTION_SENSITIVITY); 
  Wire.endTransmission();

  // Duration
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MOT_DUR);
  Wire.write(1); 
  Wire.endTransmission();
}
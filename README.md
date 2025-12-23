# Magic Wand IoT Project

> **Status:** Spouse-Approved ✅

Control smart plugs with a wave. Built to add a touch of magic to Christmas lights.

![Magic Wand Demo GIF](./.img/hero-wand-demo.gif)

## What This Is

A battery-powered motion-sensing wand that toggles Tuya smart plugs wirelessly. Wave the wand, lights respond in under 200ms. Runs for weeks on three AAA batteries.

The system uses ESP-NOW for direct device communication, an MPU-6050 accelerometer for motion detection, and deep sleep for power efficiency.

## Architecture

```
graph LR
    subgraph Wand [Wand (Sender)]
        ESP32[ESP32-C3] -- I2C --> MPU[MPU-6050]
        ESP32 -- "ESP-NOW (Ch 13)" --> Receiver
    end

    subgraph Server_Side [Server Side]
        Receiver[Receiver Dongle] -- "Serial (USB)" --> Script[Python Script]
        Script -- "TinyTuya (Local Network)" --> Plug[Smart Plug]
    end

    style Wand fill:#f9f,stroke:#333,stroke-width:2px
    style Server_Side fill:#bbf,stroke:#333,stroke-width:2px
```

**Flow**: Wand detects motion → sends ESP-NOW packet → receiver forwards via serial → Python script on homeserver toggles plug

## Development Journey

This started as a way to make Christmas more magical for my spouse. I had two ESP boards (ESP32-C3 and regular ESP32) sitting unused from a university course, and discovered that Tuya smart plugs have a local API.

### Stage 1: First Experiments

![Two ESP boards - ESP32-C3 and regular ESP32](./.img/stage1-boards.jpg)

First time actually using these boards. Spent hours getting them to communicate via ESP-NOW, learning about WiFi channels and the differences between ESP32 variants. Eventually got them talking to each other and then to my homeserver via WiFi.

### Stage 2: Breadboard Prototype

![Breadboard with accelerometer](./.img/stage2-breadboard.jpg)

Connected the MPU-6050 accelerometer to the ESP32-C3 on a breadboard. Still tethered to the computer via USB, but it worked.

![Demo of waving breadboard to control lights](./.img/breadboard-demo.gif)

Wave the breadboard, lights respond. Ridiculous looking, but proof the concept works.

### Stage 3: Going Portable

![Soldered accelerometer and ESP32-C3](./.img/stage3-soldered.jpg)

My first time soldering. Attached the MPU-6050 directly to the ESP32-C3 Supermini.

![Final soldered assembly](./.img/stage3-final.jpg)

I did't want to risk damaging the board by unsoldering the pre-installed headers, so I soldered the cables directly to the pins. To prevent short circuits in such a tight space, I bent the header pins in alternating directions (inward and outward) to maximize the space between connections.

The result is compact and surprisingly robust.

### Stage 4: Battery Power

![Wand taped to batteries](./.img/stage4-batteries.jpg)

Taped it to batteries (elegance comes later). Now it's fully wireless and portable.

## Hardware

**Wand Components:**

- ESP32-C3 Supermini
- MPU-6050 accelerometer
- 3x AAA batteries

**Receiver:**

- ESP32 DevKit (acts as USB dongle)

**Wiring:**

```
MPU-6050    ESP32-C3
────────    ────────
SDA    →    GPIO 8
SCL    →    GPIO 9
INT    →    GPIO 4
VCC    →    3.3V
GND    →    GND
```

## Key Technical Concepts

### ESP-NOW Protocol

ESP-NOW allows direct peer-to-peer communication between ESP devices without a WiFi router. Perfect for low-latency control applications. Both devices must be on the same WiFi channel (13 in this project). Channel 13 was chosen because it's not allowed in the USA but permitted in Europe, resulting in low congestion and reliable communication.

Initially, I tried having the receiver ESP32 communicate directly with the server over WiFi. However, the ESP32 uses a single chip for both WiFi and ESP-NOW, so it can't listen for ESP-NOW packets while sending WiFi commands. This caused missed messages. The solution was to connect the receiver to the homeserver via USB serial, this also reduced latency significantly.

### Deep Sleep & Power Optimization

The wand consumes only 5-10µA while sleeping. It wakes on motion detection, sends the message, and immediately returns to sleep. This gives weeks of battery life.

### Motion Detection

Instead of constantly polling the accelerometer (power hungry), the MPU-6050 is configured to trigger a hardware interrupt when motion exceeds a threshold. The wand literally sleeps until you wave it.

### Burst Transmission for Reliability

Wireless is inherently unreliable. The wand sends each message 3 times with 2ms delays between packets. The homeserver script debounces these duplicate messages, so a single wave only triggers one toggle.

### Local Tuya Control

The TinyTuya library communicates directly with the smart plug on your local network. No cloud dependency means faster response and better reliability.

### Schedule Management

The Python server automatically toggles the lights on/off at specific times (morning and evening windows). The wand can override these scheduled states at any time for manual control.

## Software Components

### Wand Firmware ([main.cpp](firmware/wand_esp-c3/src/main.cpp))

1. Wakes on MPU-6050 motion interrupt
2. Clears interrupt status
3. Sends 3-burst "TOGGLE" message via ESP-NOW
4. Reconfigures MPU for next wake
5. Returns to deep sleep

### Receiver Firmware ([receiver_esp32.ino](firmware/receiver_esp32/receiver_esp32.ino))

Simple forwarder. Listens for ESP-NOW packets on Channel 13 and sends "TOGGLE" commands over serial.

### Python Server ([magic_server.py](server/magic_server.py))

- Monitors serial port for TOGGLE commands
- Controls Tuya plug via local API
- Enforces scheduled on/off times
- Maintains state synchronization
- Implements 0.5s debounce cooldown

## Setup Instructions

### 1. Flash the Firmware

**Wand (PlatformIO):**

```bash
cd firmware/wand_esp-c3
pio run --target upload
```

**Receiver (Arduino IDE):**
Open `firmware/receiver_esp32/receiver_esp32.ino` and upload to your ESP32.

### 2. Configure MAC Address

Find your receiver's MAC address (printed during boot) and update `receiverMAC` in [main.cpp:10](firmware/wand_esp-c3/src/main.cpp#L10).

### 3. Setup the Server

```bash
cd server
cp .env.example .env
# Edit .env with your Tuya device credentials
```

Get Tuya credentials using the [TinyTuya setup wizard](https://github.com/jasonacox/tinytuya).

> [!NOTE]
> Getting local keys from Tuya requires a developer account. It takes about 10 minutes but is a one-time process. The TinyTuya wizard handles most of the heavy lifting.

### 4. Run the Server

**Via Docker (recommended):**

```bash
docker-compose up -d
```

## Configuration

| Parameter            | Location                                             | Description                                         |
| -------------------- | ---------------------------------------------------- | --------------------------------------------------- |
| `MOTION_SENSITIVITY` | [main.cpp:21](firmware/wand_esp-c3/src/main.cpp#L21) | Motion threshold (20=very sensitive, 60=hard shake) |
| `WIFI_CHANNEL`       | [main.cpp:11](firmware/wand_esp-c3/src/main.cpp#L11) | Must match between wand and receiver                |
| `receiverMAC`        | [main.cpp:10](firmware/wand_esp-c3/src/main.cpp#L10) | Receiver's MAC address                              |
| `TOGGLE_COOLDOWN`    | [magic_server.py:14](server/magic_server.py#L14)     | Debounce time in seconds                            |
| `ON_WINDOWS`         | [magic_server.py:19](server/magic_server.py#L19)     | Scheduled on times (24h format)                     |

## Technical Specs

- **Latency**: 100-200ms from wave to light toggle
- **Range**: 10-20m line of sight
- **Battery Life**: Weeks to months
- **Communication**: ESP-NOW on 2.4GHz WiFi ↔ USB Serial (115200 baud)

## What I Learned

This project was a hands-on crash course in embedded IoT:

- **ESP-NOW protocol** - Direct peer-to-peer communication without infrastructure
- **WiFi channels** - How to synchronize and configure channels explicitly
- **Deep sleep techniques** - RTC memory, GPIO wakeup, minimal wake time
- **Hardware interrupts** - Using the MPU-6050's built-in motion detection
- **Motion detection registers** - Threshold, duration, and high-pass filtering
- **Wireless reliability** - Burst transmission and debouncing strategies
- **Tuya local API** - Controlling smart devices without cloud services
- **Serial communication** - ESP-to-computer data forwarding
- **Battery optimization** - Every microamp counts
- **Soldering** - My first time, learned on this project

## Future Ideas

- Web interface for customization of automatic schedules
- Gesture recognition (different waves for different actions)
- 3D printed wand enclosure
- Battery level monitoring and warnings

## License

MIT License - See [LICENSE](LICENSE)

---

_Built with curiosity and a desire to make everyday life a little more magical._

# ESP32-Drone

A compact DIY drone project using a **generic ESP32 development board as both the flight controller and onboard receiver**, with **ESP-NOW** used to communicate with a separate ESP32 transmitter.

The project combines an ESP32 flight-controller firmware, an MPU6050 IMU, and an ESP-NOW control link to create a low-cost drone platform without a dedicated commercial flight controller or ELRS receiver.

## Requirements

### Hardware

- 2× ESP32 development boards
  - 1× ESP32 as the flight controller + onboard receiver
  - 1× ESP32 as the ESP-NOW transmitter
- MPU6050
- Motors, motor driver/MOSFET stage, battery, and frame according to your build
- Buzzer and other electronic components *(optional)*

## Software

### 1. ESP-IDF v5.0.7

Install **ESP-IDF v5.0.7**:

https://dl.espressif.com/dl/esp-idf/

### 2. Betaflight Configurator v10.10.0

Download **Betaflight Configurator 10.10.0**:

https://github.com/betaflight/betaflight-configurator/releases/tag/10.10.0

Betaflight Configurator is used to configure and monitor the ESP32-based flight controller.

### 3. Firmware Upload

For uploading the firmware to the ESP32, use Espressif's browser-based `esptool-js`:

https://espressif.github.io/esptool-js/


## Connections

### Flight Controller — ESP32 to MPU6050

| ESP32 GPIO | MPU6050 |
|---:|---|
| 3V3 | Vin |
| GND | GND |
| GPIO 21 | SDA |
| GPIO 22 | SCL |
| GPIO 23 | INT |

### ESP-NOW Transmitter — ESP32 to Controls

| ESP32 GPIO | Control Signal |
|---:|---|
| GPIO 34 | JOY1 VRx (Throttle) |
| GPIO 35 | JOY1 VRy (Yaw) |
| GPIO 33 | JOY2 VRx (Roll) |
| GPIO 32 | JOY2 VRy (Pitch) |
| GPIO 25 | Toggle Switch (AUX1) |
| 3V3 | Joysticks + Switch VCC |
| GND | Joysticks + Switch GND |

## How It Works

The onboard ESP32 handles both:

- Flight-controller processing
- ESP-NOW receiver functionality

A second ESP32 acts as the transmitter and sends control data wirelessly using **ESP-NOW**.

The overall architecture is:

```text
                 ESP-NOW
    ESP32 Transmitter ───────────► ESP32 Flight Controller
                                      │
                                      ├── MPU6050
                                      ├── Betaflight
                                      └── Motor Outputs
```

## Installation & Setup

The detailed installation, firmware flashing, wiring, Betaflight configuration, ESP-NOW transmitter setup, and complete working procedure will be covered in the project video.

> **YouTube video:**
>
> *Video is currently under construction.*

## Credits

This project builds upon the following open-source projects:

1. **ESP-FC** — ESP32 flight controller  
   https://github.com/rtlopez/esp-fc

2. **Mini ESP-NOW RC Drone** — ESP32 ESP-NOW transmitter  
   https://github.com/nikhiltelase/mini-esp-now-rc-drone

Please refer to the original repositories for their respective licenses, documentation, and implementation details.

## Project Status

🚧 **Under development**

The hardware, firmware, and flight-control setup are being developed and tested incrementally.

# LoRa Smart Irrigation System Prototype

A long-range smart irrigation system built using **Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)** modules.  
Operates fully **offline**, with no WiFi or internet connection required—ideal for farms and remote areas.

---

## System Architecture

The system includes **four LoRa (915 MHz) nodes**:

### Control Central Unit
- Receives soil moisture data from sensors.
- Displays system status on an I2C LCD.
- Sends irrigation commands to the Tank Unit.
- Supports manual/simulation mode.

### Water Tank
- Controls irrigation valves (relays).
- Reads the water level using a float switch.
- Executes irrigation commands with built-in safety logic.

### Two Soil Sensors
- Remote nodes that measure soil moisture in irrigation zones **F1** and **F2**.
- Transmit data periodically via LoRa to the Receiver Hub.

---

## Hardware Connections (Pinout)

### 1. Control Central Unit

| Component        | Heltec Pin | Description |
|------------------|------------|-------------|
| LCD I2C (SDA)    | GPIO 19    | Physical pin 18 (Right side) |
| LCD I2C (SCL)    | GPIO 20    | Physical pin 17 (Right side) |
| Button           | GPIO 6     | Wakes screen (30s timeout) |
| Potentiometer 1  | GPIO 1     | Soil Moisture F1 simulation |
| Potentiometer 2  | GPIO 2     | Soil Moisture F2 simulation |

**Logic:**
- If `Moisture < 50%` → irrigation command is sent.  
- If the tank is empty → onboard OLED flashes **"NO WATER"**.

---

### 2. Water Tank

| Component        | Heltec Pin | Description |
|------------------|------------|-------------|
| Float Sensor     | GPIO 4     | Water level input |
| Valve 1 (Relay)  | GPIO 26    | Irrigation Zone 1 |
| Valve 2 (Relay)  | GPIO 47    | Irrigation Zone 2 |

**Logic:**
- Receives humidity data in format: `H:xx,xx`
- Threshold: moisture < **50%** → valve ON  
- If float switch reports *Empty* → **all valves forced OFF (safety override)**

---

### 3. Soil Sensors

| Component        | Heltec Pin | Description |
|------------------|------------|-------------|
| Capacitive Sensor | GPIO 4     | Analog output (yellow wire) |

---

## Software Configuration

### Required Libraries

Install via Arduino IDE Library Manager:

- **RadioLib** (by Jan Gromeš)
- **LiquidCrystal I2C** (by Frank de Brabander)
- **Heltec ESP32 Dev-Boards** (Board Manager)

### Board Settings

- **Board:** Heltec WiFi LoRa 32 (V3)  
- **USB CDC On Boot:** Enabled (required for Serial Monitor)

---

## Receiver Hub Modes

Mode selection is done through the **Serial Monitor**:

### **Mode 0 – Sensor Mode**
- Uses real LoRa data from the Soil Sensor nodes.

### **Mode 1 – Manual / Simulation Mode**
- Ignores LoRa soil moisture data.
- Uses local potentiometers to simulate F1 and F2 humidity.
- Useful for testing irrigation logic without field sensors.



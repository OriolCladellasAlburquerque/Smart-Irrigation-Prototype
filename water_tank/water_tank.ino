#include <Wire.h>
#include <RadioLib.h>
#include "SSD1306Wire.h"

// --- CONFIGURATION ---
#define ID_SENSOR  3   
#define FREQUENCY 915.0 
const int MOISTURE_THRESHOLD = 50; // Tank decides based on this

// --- PINS ---
#define PIN_FLOAT      4   // Float Sensor
#define PIN_VALVE_1    26  // Valve 1 (F1)
#define PIN_VALVE_2    47  // Valve 2 (F2)

// --- RELAY LOGIC (Active LOW) ---
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// LoRa Module
#define LORA_NSS  8
#define LORA_DIO1 14
#define LORA_RST  12
#define LORA_BUSY 13

SSD1306Wire display(0x3c, SDA_OLED, SCL_OLED, GEOMETRY_128_64);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

void VextON() { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW); }

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(1));
  VextON(); delay(100);
  
  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, LOW); delay(50); digitalWrite(RST_OLED, HIGH); delay(50);

  // Initialize Display
  display.init(); 
  display.flipScreenVertically(); 
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Starting Tank..."); 
  display.display();

  // Configure Pins
  pinMode(PIN_FLOAT, INPUT_PULLUP);
  pinMode(PIN_VALVE_1, OUTPUT);
  pinMode(PIN_VALVE_2, OUTPUT);
  
  // Close valves initially
  digitalWrite(PIN_VALVE_1, RELAY_OFF);
  digitalWrite(PIN_VALVE_2, RELAY_OFF);

  int state = radio.begin(FREQUENCY);
  if (state == RADIOLIB_ERR_NONE) Serial.println("Radio OK!");
  else while(true);
  
  radio.setOutputPower(22);
}

void loop() {
  // 1. READ FLOAT SWITCH
  int reading = digitalRead(PIN_FLOAT);
  // Logic: LOW (Connected to GND) = HAS WATER (1)
  int hasWater = (reading == LOW) ? 1 : 0; 

  // 2. SEND STATUS
  // Send S3:1 (Has Water) or S3:0 (Empty)
  // Note: We send status regardless, but logic applies locally too.
  String message = "S3:" + String(hasWater); 
  Serial.print("TX: " + message);
  radio.transmit(message);

  // 3. WAIT FOR HUMIDITY DATA (Timeout 4s)
  Serial.print(" -> Waiting for Data... ");
  String response;
  bool dataReceived = false;
  unsigned long startTime = millis();
  
  radio.startReceive();
  while (millis() - startTime < 4000) { 
    if (radio.getIrqFlags() & RADIOLIB_SX126X_IRQ_RX_DONE) {
       if (radio.readData(response) == RADIOLIB_ERR_NONE) dataReceived = true;
       break; 
    }
  }

  // 4. PROCESS DATA
  if (dataReceived) {
    if (response.startsWith("H:")) {
      int commaIndex = response.indexOf(",");
      if (commaIndex > 0) {
        int h1 = response.substring(2, commaIndex).toInt();
        int h2 = response.substring(commaIndex + 1).toInt();
        
        bool v1 = false;
        bool v2 = false;

        // VALVE LOGIC (Only if water is present)
        if (hasWater == 1) {
            // Valve 1
            if (h1 != -1 && h1 < MOISTURE_THRESHOLD) { 
               digitalWrite(PIN_VALVE_1, RELAY_ON); v1 = true; 
            } else { 
               digitalWrite(PIN_VALVE_1, RELAY_OFF); 
            }
    
            // Valve 2
            if (h2 != -1 && h2 < MOISTURE_THRESHOLD) { 
               digitalWrite(PIN_VALVE_2, RELAY_ON); v2 = true; 
            } else { 
               digitalWrite(PIN_VALVE_2, RELAY_OFF); 
            }
        } else {
            // SAFETY: Force close if empty
            digitalWrite(PIN_VALVE_1, RELAY_OFF);
            digitalWrite(PIN_VALVE_2, RELAY_OFF);
        }
        
        // DISPLAY STATUS
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        display.drawString(0, 0, "STATUS (H<" + String(MOISTURE_THRESHOLD) + "%)");
        
        display.setFont(ArialMT_Plain_16);
        String s1 = v1 ? "OPEN" : "CLOSED";
        display.drawString(0, 20, "V1: " + s1 + " (" + String(h1) + "%)");
        
        String s2 = v2 ? "OPEN" : "CLOSED";
        display.drawString(0, 42, "V2: " + s2 + " (" + String(h2) + "%)");
        
        // Empty Warning
        if (hasWater == 0) {
           display.setTextAlignment(TEXT_ALIGN_RIGHT);
           display.setFont(ArialMT_Plain_10);
           display.drawString(128, 0, "!NO WATER!");
        }
        display.display();
      }
    }
  } else {
    // Timeout or No Signal
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 20, "NO SIGNAL FROM HUB");
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 40, "Valves Closed");
    display.display();
    
    // Safety Close
    digitalWrite(PIN_VALVE_1, RELAY_OFF);
    digitalWrite(PIN_VALVE_2, RELAY_OFF);
  }

  // Random Wait (IMPORTANT to avoid collissions)
  delay(2000 + random(0, 1000));
}
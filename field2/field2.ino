#include <Wire.h>
#include <RadioLib.h>

// --- SENSOR IDENTIFICATION ---
#define ID_SENSOR  2   

// Configuration
#define FREQUENCY  915.0 
#define PIN_SENSOR  4   

// Calibration (Inverse Logic for Capacitive Sensors)
const int DRY = 3000;   
const int WET = 1200;   

// LoRa Pins
#define LORA_NSS  8
#define LORA_DIO1 14
#define LORA_RST  12
#define LORA_BUSY 13

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); 
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(1));

  VextON();
  delay(100); 
  
  // Power up radio, but we don't init the display
  
  pinMode(PIN_SENSOR, ANALOG);
  analogReadResolution(12);       
  analogSetAttenuation(ADC_11db);

  int state = radio.begin(FREQUENCY);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Radio OK!");
  } else {
    while (true);
  }
  
  radio.setOutputPower(22); 
  delay(1000);
}

void loop() {
  // 1. Read Sensor
  int rawValue = analogRead(PIN_SENSOR);
  int percentage = map(rawValue, DRY, WET, 0, 100);
  
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;

  // 2. Send Message: "S1:45"
  String message = "S" + String(ID_SENSOR) + ":" + String(percentage);
  
  Serial.print("Sending: "); Serial.println(message);
  radio.transmit(message);

  // 3. Random Sleep
  delay(2000 + random(0, 1000)); 
}
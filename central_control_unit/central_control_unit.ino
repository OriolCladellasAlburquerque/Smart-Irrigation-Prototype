#include <Wire.h>
#include <RadioLib.h>
#include <LiquidCrystal_I2C.h> 

#define FREQUENCY  915.0 

// --- IRRIGATION CONFIGURATION ---
const int MOISTURE_THRESHOLD = 50; // Irrigates if humidity is below 50%

// --- PINS ---
#define LCD_SDA     19  
#define LCD_SCL     20  
#define PIN_BUTTON  6   // Button on Pin 6

// --- SIMULATION PINS ---
#define PIN_POT1    1   
#define PIN_POT2    2   

// --- STATE VARIABLES ---
bool simulationMode = false;
bool valve1Open = false;
bool valve2Open = false;

// LCD Display
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// LoRa Module
#define LORA_NSS  8
#define LORA_DIO1 14
#define LORA_RST  12
#define LORA_BUSY 13

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Sensor Data
int valS1 = -1; // Moisture Field 1
int valS2 = -1; // Moisture Field 2
int valS3 = -1; // Tank Level

// Display Timer
unsigned long lastActivityTime = 0;
bool displayOn = true;
const unsigned long DISPLAY_TIMEOUT = 30000; // 30 seconds

// --- INTERRUPTS ---
volatile bool buttonPressed = false;
volatile bool packetReceived = false;

void IRAM_ATTR ISR_Button() { buttonPressed = true; }
void IRAM_ATTR setFlag() { packetReceived = true; }

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

// --- LOGIC: IRRIGATION BRAIN ---
void calculateIrrigation() {
  // SAFETY: If tank is empty (1) or unknown (-1), STOP ALL
  if (valS3 != 0) { // 0 = Full, 1 = Empty
    valve1Open = false;
    valve2Open = false;
    return; 
  }
  
  // If there is water, check moisture levels
  
  // Decide Valve 1
  if (valS1 != -1 && valS1 < MOISTURE_THRESHOLD) valve1Open = true;
  else valve1Open = false;

  // Decide Valve 2
  if (valS2 != -1 && valS2 < MOISTURE_THRESHOLD) valve2Open = true;
  else valve2Open = false;
}

// --- LCD DISPLAY ---
void updateLCD() {
  if (!displayOn) return;

  lcd.setCursor(0, 0);
  
  // Field 1 (F1)
  lcd.print("F1:");
  if (valS1 == -1) lcd.print("--"); else lcd.print(valS1);
  lcd.print("%");
  
  // Dynamic spacing
  int pos = 3 + ((valS1==-1)?2:String(valS1).length()) + 1;
  while(pos < 8) { lcd.print(" "); pos++; }

  // Field 2 (F2)
  lcd.print("F2:");
  if (valS2 == -1) lcd.print("--"); else lcd.print(valS2);
  lcd.print("%");

  pos = 8 + 3 + ((valS2==-1)?2:String(valS2).length()) + 1;
  while(pos < 15) { lcd.print(" "); pos++; }
  
  // Mode Indicator (M=Manual, S=Sensor)
  lcd.setCursor(15, 0);
  if (simulationMode) lcd.print("M"); else lcd.print("S");                

  // Line 2: Tank State
  lcd.setCursor(0, 1);
  String stateStr = "-----";
  if (valS3 == 0) stateStr = "Full ";  // 0 is Full
  else if (valS3 == 1) stateStr = "Empty"; // 1 is Empty
  
  lcd.print("Tank State:" + stateStr);
}

void turnDisplayOn() {
  if (!displayOn) {
    lcd.backlight(); 
    lcd.display();
    displayOn = true;
  }
  lastActivityTime = millis();
  updateLCD();
}

void turnDisplayOff() {
  if (displayOn) {
    lcd.noBacklight(); 
    lcd.noDisplay();
    displayOn = false;
  }
}

// --- SIMULATION FUNCTION ---
void readPotentiometers() {
  // Read pots (0-4095) and map to % (0-100) using inverse logic for capacitance simulation
  // 3000 (Dry) -> 0%, 1200 (Wet) -> 100%
  // Or linear map if using standard pots:
  int raw1 = analogRead(PIN_POT1);
  int raw2 = analogRead(PIN_POT2);
  
  // Assuming standard pots for simulation (0V=0%, 3.3V=100%)
  valS1 = map(raw1, 0, 4095, 0, 100);
  valS2 = map(raw2, 0, 4095, 0, 100);
  
  // Simulation: Assume tank is Full (0) by default
  if (valS3 == -1) valS3 = 0; 
  
  calculateIrrigation();
  updateLCD();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_POT1, ANALOG);
  pinMode(PIN_POT2, ANALOG);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Interrupt on Change (Press or Release)
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), ISR_Button, CHANGE);

  VextON(); 
  delay(100);
  
  // Initialize LCD
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init(); 
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Irrigation Hub");
  lcd.setCursor(0, 1); lcd.print("Starting...");

  // Initialize LoRa
  int state = radio.begin(FREQUENCY);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Radio OK");
    radio.setDio1Action(setFlag); 
    radio.startReceive();         
  } else {
    lcd.setCursor(0, 1); lcd.print("Radio Error!");
    while (true);
  }
  
  lastActivityTime = millis();
}

void loop() {
  // 1. BUTTON MANAGEMENT
  if (buttonPressed) {
    buttonPressed = false; 
    turnDisplayOn();   
    delay(50); // Debounce
  }

  // 2. MODE CHANGE (Via Serial)
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') { simulationMode = true; turnDisplayOn(); Serial.println("MODE: MANUAL"); }
    if (c == '0') { simulationMode = false; turnDisplayOn(); radio.startReceive(); Serial.println("MODE: SENSOR"); }
  }

  // 3. MAIN LOGIC
  if (simulationMode) {
    if (displayOn) {
      readPotentiometers();
      delay(100); 
    }
  } 
  else {
    // SENSOR MODE
    if (packetReceived) {
      packetReceived = false;
      String message;
      if (radio.readData(message) == RADIOLIB_ERR_NONE) {
        Serial.println("RX: " + message);
        
        // Parse incoming data
        if (!simulationMode) {
          if (message.startsWith("S1:")) valS1 = message.substring(3).toInt();
          if (message.startsWith("S2:")) valS2 = message.substring(3).toInt();
        }

        if (message.startsWith("S3:")) {
           // Tank Message Received
           valS3 = message.substring(3).toInt();
           
           calculateIrrigation();
           
           // REPLY TO TANK: SEND HUMIDITY DATA (H:h1,h2)
           delay(200); // Wait for tank to switch to RX
           String moistureData = "H:" + String(valS1) + "," + String(valS2);
           Serial.println("TX Data: " + moistureData);
           radio.transmit(moistureData);
           radio.startReceive(); // Back to listening
        } else {
           calculateIrrigation();
        }

        if (displayOn) updateLCD(); 
      }
    }
  }

  // 4. AUTO POWER OFF
  if (displayOn && (millis() - lastActivityTime > DISPLAY_TIMEOUT)) {
    turnDisplayOff();
  }
}

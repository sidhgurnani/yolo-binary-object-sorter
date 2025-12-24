#include <Arduino.h>
#include <EEPROM.h>
#include <Servo.h>
#include <LiquidCrystal.h>

// ===== HARDWARE CONFIGURATION =====
// TODO: Update these pin numbers to match your wiring

// Servo Configuration - change angle values as needed
const int SERVO_PIN = 3;              // PWM pin for servo
const int TARGET_ANGLE = 135;          // Servo angle for target pile
const int NOT_TARGET_ANGLE = 53;       // Servo angle for other pile  
const int NEUTRAL_ANGLE = 93;         // Servo neutral/center position

// LED Configuration
const int GREEN_LED_PIN = 10;          // Green LED (ready to sort)
const int RED_LED_PIN = 11;            // Red LED (stop/processing)

// LCD Configuration (1602 Display)
// LCD RS, E, D4, D5, D6, D7
const int LCD_RS = 13;
const int LCD_E = 12;
const int LCD_D4 = 4;
const int LCD_D5 = 5;
const int LCD_D6 = 6;
const int LCD_D7 = 7;

// ===== HARDWARE OBJECTS =====
Servo sortingServo;
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ===== STORAGE CONSTANTS =====
const uint8_t MAX_ITEMS = 30;
const uint8_t MAX_STR_LEN = 50;
const int EEPROM_START_ADDR = 0;
const int NUM_ITEMS_ADDR = 0;
const int ITEMS_START_ADDR = 1;

// ===== STATE VARIABLES =====
String storedItems[MAX_ITEMS];
uint8_t numStoredItems = 0;
bool itemsLoaded = false;

// Binary Sorting State
String currentTargetClass = "";
bool binarySortActive = false;

// ========================================
// SERVO-BASED COUNTING (GROUND TRUTH)
// ========================================
int currentPassTargetCount = 0;     // Target objects in current pass
int currentPassOtherCount = 0;      // Non-target objects in current pass
int totalSessionServoCount = 0;     // Total across all passes
String lastSortedItem = "";
unsigned long lastSortTime = 0;
const unsigned long SORT_COOLDOWN = 1500; // 1.5 seconds between operations

// Mode tracking
String currentMode = "";
String lastModeMessage = "";
unsigned long lastModeTime = 0;
const unsigned long MODE_MESSAGE_COOLDOWN = 5000;

// LCD State
String currentLCDMessage = "";
unsigned long lastLCDUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 500; // Update LCD every 500ms

// ===== FUNCTION DECLARATIONS =====
void performTargetSortAction();
void performOtherSortAction();
void updateLCD(String line1, String line2);
void setLEDState(bool greenOn, bool redOn);
void returnServoToNeutral();
void incrementServoCount();

// ===== EEPROM FUNCTIONS =====
void storeObjectsInEEPROM(String objects[], uint8_t count) {
  EEPROM.write(NUM_ITEMS_ADDR, count);
  
  int addr = ITEMS_START_ADDR;
  for (uint8_t i = 0; i < count; i++) {
    String item = objects[i];
    uint8_t len = min(item.length(), (unsigned int)(MAX_STR_LEN - 1));
    EEPROM.write(addr, len);
    addr++;
    
    for (uint8_t j = 0; j < len; j++) {
      EEPROM.write(addr, item[j]);
      addr++;
    }
    
    for (uint8_t j = len; j < MAX_STR_LEN - 1; j++) {
      EEPROM.write(addr, 0);
      addr++;
    }
  }
  
  Serial.println("✅ Objects stored in EEPROM");
  updateLCD("Objects Stored", String(count) + " items");
}

void loadObjectsFromEEPROM() {
  numStoredItems = EEPROM.read(NUM_ITEMS_ADDR);
  
  if (numStoredItems > MAX_ITEMS) {
    numStoredItems = 0;
    Serial.println("⚠️ Invalid EEPROM data");
    updateLCD("EEPROM Error", "Data invalid");
    return;
  }
  
  if (numStoredItems == 0) {
    Serial.println("📭 No objects in EEPROM");
    updateLCD("Ready", "No objects");
    return;
  }
  
  int addr = ITEMS_START_ADDR;
  for (uint8_t i = 0; i < numStoredItems; i++) {
    uint8_t len = EEPROM.read(addr);
    addr++;
    
    if (len >= MAX_STR_LEN) len = 0;
    
    String item = "";
    for (uint8_t j = 0; j < len; j++) {
      char c = EEPROM.read(addr);
      if (c != 0) item += c;
      addr++;
    }
    
    addr += (MAX_STR_LEN - 1 - len);
    storedItems[i] = item;
  }
  
  if (numStoredItems > 0) {
    itemsLoaded = true;
    Serial.println("📥 Objects loaded:");
    for (uint8_t i = 0; i < numStoredItems; i++) {
      Serial.print("  ");
      Serial.println(storedItems[i]);
    }
    updateLCD("Objects Loaded", String(numStoredItems) + " items");
    Serial.println("READY_TO_SORT");
  }
}

void processObjectList(String objectListString) {
  String tempItems[MAX_ITEMS];
  uint8_t count = 0;
  
  int startIdx = 0;
  int commaIdx = objectListString.indexOf(',');
  
  while (commaIdx != -1 && count < MAX_ITEMS) {
    tempItems[count] = objectListString.substring(startIdx, commaIdx);
    tempItems[count].trim();
    count++;
    startIdx = commaIdx + 1;
    commaIdx = objectListString.indexOf(',', startIdx);
  }
  
  if (startIdx < (int)objectListString.length() && count < MAX_ITEMS) {  // Cast to int
    tempItems[count] = objectListString.substring(startIdx);
    tempItems[count].trim();
    count++;
  }
  
  if (count > 0) {
    storeObjectsInEEPROM(tempItems, count);
    loadObjectsFromEEPROM();
  }
}

// ===== LCD CONTROL =====
void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16)); // Limit to 16 chars
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
  currentLCDMessage = line1 + "|" + line2;
}

// ===== LED CONTROL =====
void setLEDState(bool greenOn, bool redOn) {
  digitalWrite(GREEN_LED_PIN, greenOn ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, redOn ? HIGH : LOW);
}

// ===== SERVO CONTROL =====
void returnServoToNeutral() {
  sortingServo.write(NEUTRAL_ANGLE);
  delay(300); // Allow servo to reach position
}

// ========================================
// SERVO COUNTING FUNCTIONS
// ========================================
void performTargetSortAction() {
  Serial.println("🎯 → TARGET pile");
  
  // Move servo
  sortingServo.write(TARGET_ANGLE);
  delay(800);
  returnServoToNeutral();
  
  // INCREMENT COUNTERS
  currentPassTargetCount++;
  totalSessionServoCount++;
  
  // Update LCD
  int total = currentPassTargetCount + currentPassOtherCount;
  updateLCD("Sorting: T" + String(currentPassTargetCount), "Total: " + String(total));
  
  // Send count back to Python
  Serial.print("SORTED_TARGET:");
  Serial.println(currentPassTargetCount);
}

void performOtherSortAction() {
  Serial.println("📦 → OTHER pile");
  
  // Move servo
  sortingServo.write(NOT_TARGET_ANGLE);
  delay(800);
  returnServoToNeutral();
  
  // INCREMENT COUNTERS
  currentPassOtherCount++;
  totalSessionServoCount++;
  
  // Update LCD
  int total = currentPassTargetCount + currentPassOtherCount;
  updateLCD("Sorting: O" + String(currentPassOtherCount), "Total: " + String(total));
  
  // Send count back to Python
  Serial.print("SORTED_OTHER:");
  Serial.println(currentPassOtherCount);
}

// ===== SORTING LOGIC =====
void handleBinarySortTarget(String itemClass) {
  unsigned long currentTime = millis();
  
  // Cooldown to prevent duplicate sorts
  if (itemClass != lastSortedItem || (currentTime - lastSortTime) > SORT_COOLDOWN) {
    Serial.print("🎯 TARGET: ");
    Serial.println(itemClass);
    
    performTargetSortAction();
    
    lastSortedItem = itemClass;
    lastSortTime = currentTime;
  } else {
    Serial.println("⏱️ Cooldown active, skipping duplicate");
  }
}

void handleBinarySortOther(String itemClass) {
  unsigned long currentTime = millis();
  
  // Cooldown to prevent duplicate sorts
  if (itemClass != lastSortedItem || (currentTime - lastSortTime) > SORT_COOLDOWN) {
    Serial.print("📦 OTHER: ");
    Serial.println(itemClass);
    
    performOtherSortAction();
    
    lastSortedItem = itemClass;
    lastSortTime = currentTime;
  } else {
    Serial.println("⏱️ Cooldown active, skipping duplicate");
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(1000);
  
  Serial.println("🤖 Arduino Binary Sorter V3 - Servo Counting");
  
  // Initialize LCD
  lcd.begin(16, 2);
  updateLCD("System Starting", "Please wait...");
  
  // Initialize LEDs
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  setLEDState(false, true); // Red LED on during startup
  
  // Initialize Servo
  sortingServo.attach(SERVO_PIN);
  sortingServo.write(NEUTRAL_ANGLE);
  delay(500);
  
  Serial.println("✅ Hardware initialized");
  
  // Load existing objects
  loadObjectsFromEEPROM();
  
  // Ready state
  updateLCD("System Ready", "Waiting...");
  setLEDState(false, false); // All LEDs off
  
  Serial.println("READY");
}

// ===== MAIN LOOP =====
void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.startsWith("STORE_OBJECTS:")) {
      String objectList = input.substring(14);
      Serial.print("📥 Storing: ");
      Serial.println(objectList);
      processObjectList(objectList);
      
    } else if (input.startsWith("SET_TARGET:")) {
        currentTargetClass = input.substring(11);
        currentPassTargetCount = 0;  // Reset target counter
        currentPassOtherCount = 0;   // Reset other counter
        binarySortActive = true;
        
        Serial.print("🎯 Target set: ");
        Serial.println(currentTargetClass);
        Serial.println("PASS_COUNTER_RESET:0");
        
        updateLCD("Target: " + currentTargetClass.substring(0, 8), "Count: 0");

    } else if (input.startsWith("SORT_TARGET:")) {
      if (binarySortActive) {
        String detectedClass = input.substring(12);
        handleBinarySortTarget(detectedClass);
      }
      
    } else if (input.startsWith("SORT_OTHER:")) {
      if (binarySortActive) {
        String detectedClass = input.substring(11);
        handleBinarySortOther(detectedClass);
      }
      
    } else if (input == "GREEN_LED_ON") {
      setLEDState(true, false);
      Serial.println("🟢 Green LED ON - Ready to feed");
      
    } else if (input == "RED_LED_ON") {
      setLEDState(false, true);
      Serial.println("🔴 Red LED ON - Stop feeding");
      
    } else if (input == "GREEN_LED_OFF") {
      setLEDState(false, false);
      Serial.println("🟢 Green LED OFF");
      
    } else if (input == "RED_LED_OFF") {
      setLEDState(false, false);
      Serial.println("🔴 Red LED OFF");
      
    } else if (input == "PAUSE_SORT") {
        Serial.println("🛑 PAUSE_SORT received");
        Serial.print("📊 Current counts - Target: ");
        Serial.print(currentPassTargetCount);
        Serial.print(", Other: ");
        Serial.println(currentPassOtherCount);
        
        // Stop sorting immediately
        binarySortActive = false;
        setLEDState(false, true);  // Red LED on
        
        Serial.println("⏳ Waiting for servo to settle...");
        // Wait for any in-progress servo movements to complete
        delay(1000);
        
        Serial.println("📤 Sending PASS_COMPLETE...");
        // Send complete pass information THREE TIMES to ensure receipt
        for (int i = 0; i < 3; i++) {
            Serial.print("PASS_COMPLETE:");
            Serial.print(currentPassTargetCount);
            Serial.print(":");
            Serial.println(currentPassOtherCount);
            delay(100);  // Small delay between sends
        }
        Serial.println("✅ PASS_COMPLETE sent 3 times");
        
        int total = currentPassTargetCount + currentPassOtherCount;
        updateLCD("Pass Complete", "Total: " + String(total));
        Serial.println("✅ Pass data sent");
        
        currentTargetClass = "";
        lastSortedItem = "";
        returnServoToNeutral();
      
    } else if (input == "FINISH_SORT") {
      binarySortActive = false;
      currentTargetClass = "";
      lastSortedItem = "";
      Serial.print("✅ Session finished - Total movements: ");
      Serial.println(totalSessionServoCount);
      updateLCD("Session Done", String(totalSessionServoCount) + " total");
      setLEDState(false, false);
      returnServoToNeutral();
      delay(2000);
      
      // Reset counters for next session
      currentPassTargetCount = 0;
      currentPassOtherCount = 0;
      totalSessionServoCount = 0;
      
      updateLCD("System Ready", "Waiting...");
      
    } else if (input == "LOAD_OBJECTS") {
      loadObjectsFromEEPROM();
      
    } else if (input == "LIST_OBJECTS") {
      if (itemsLoaded && numStoredItems > 0) {
        Serial.println("📋 Object list:");
        for (uint8_t i = 0; i < numStoredItems; i++) {
          Serial.print("  ");
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.println(storedItems[i]);
        }
        if (binarySortActive) {
          Serial.print("🎯 Current target: ");
          Serial.println(currentTargetClass);
          Serial.print("🔧 Pass target count: ");
          Serial.println(currentPassTargetCount);
          Serial.print("🔧 Pass other count: ");
          Serial.println(currentPassOtherCount);
          int total = currentPassTargetCount + currentPassOtherCount;
          Serial.print("🔧 Pass total: ");
          Serial.println(total);
        }
      } else {
        Serial.println("🔭 No objects stored");
      }
      
    } else if (input == "CLEAR_OBJECTS") {
      EEPROM.write(NUM_ITEMS_ADDR, 0);
      numStoredItems = 0;
      itemsLoaded = false;
      binarySortActive = false;
      currentTargetClass = "";
      currentPassTargetCount = 0;
      currentPassOtherCount = 0;
      totalSessionServoCount = 0;
      Serial.println("🗑️ Objects cleared & counters reset");
      updateLCD("Objects Cleared", "Memory empty");
    } else if (input == "stop") {
      binarySortActive = false;
      Serial.println("🛑 Stopped");
      updateLCD("System Stopped", "Standby mode");
      setLEDState(false, false);
      returnServoToNeutral();
      lastSortedItem = "";
      
    } else {
      Serial.print("❓ Unknown: ");
      Serial.println(input);
    }
  }
  
  // Periodic LCD updates during active sorting
  unsigned long currentTime = millis();
  if (binarySortActive && currentTargetClass != "" && 
      (currentTime - lastLCDUpdate) > LCD_UPDATE_INTERVAL) {
    
    String statusLine1 = "Tgt:" + currentTargetClass.substring(0, 12);
    int totalPass = currentPassTargetCount + currentPassOtherCount;
    String statusLine2 = "T:" + String(currentPassTargetCount) + " O:" + String(currentPassOtherCount);
    
    if (currentLCDMessage != (statusLine1 + "|" + statusLine2)) {
      updateLCD(statusLine1, statusLine2);
    }
    
    lastLCDUpdate = currentTime;
  }
}
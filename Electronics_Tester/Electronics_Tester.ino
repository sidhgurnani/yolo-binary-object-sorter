/*
 * Servo Position Tuner with LED & LCD - Arduino Leonardo
 * 
 * Hardware:
 * - Servo on pin 3
 * - Green LED on pin 10
 * - Red LED on pin 11
 * - LCD: RS=13, E=12, DB4=4, DB5=5, DB6=6, DB7=7
 * 
 * Commands:
 * - 0-180: Move to position
 * - 'h': Set HOME position
 * - 'l': Set LOW limit
 * - 'u': Set UPPER limit
 * - 's': Start sweep
 * - 'x': Stop sweep
 * - 'p': Print settings
 * - 'g': Go to HOME
 */

#include <Servo.h>
#include <LiquidCrystal.h>

// Pin definitions
const int servoPin = 3;
const int greenLED = 10;
const int redLED = 11;

// LCD: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(13, 12, 4, 5, 6, 7);

Servo myServo;

// Position variables
int currentPos = 90;
int homePos = 90;
int lowLimit = 0;
int upperLimit = 180;

// Sweep variables
bool isSweeping = false;
int sweepDirection = 1;
int sweepSpeed = 15;

// LED timing
unsigned long lastLEDToggle = 0;
bool greenState = true;

// Display state
bool showingWelcome = true;

void setup() {
  Serial.begin(9600);
  
  // Setup LEDs
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  digitalWrite(greenLED, HIGH);
  digitalWrite(redLED, LOW);
  
  // Setup servo
  myServo.attach(servoPin);
  delay(100);
  myServo.write(currentPos);
  
  // Setup LCD
  lcd.begin(16, 2);
  delay(50);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  SERVO TUNER");
  lcd.setCursor(0, 1);
  lcd.print("  Welcome!");
  
  Serial.println("=== Servo Position Tuner ===");
  Serial.println("Commands:");
  Serial.println("  0-180: Move to position");
  Serial.println("  h: Set HOME position");
  Serial.println("  l: Set LOW limit");
  Serial.println("  u: Set UPPER limit");
  Serial.println("  s: Start sweep");
  Serial.println("  x: Stop sweep");
  Serial.println("  p: Print settings");
  Serial.println("  g: Go to HOME");
  Serial.println("===========================");
  printSettings();
}

void loop() {
  // Handle LED blinking (every 1 second)
  unsigned long currentMillis = millis();
  if (currentMillis - lastLEDToggle >= 1000) {
    lastLEDToggle = currentMillis;
    greenState = !greenState;
    
    if (greenState) {
      digitalWrite(greenLED, HIGH);
      digitalWrite(redLED, LOW);
    } else {
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, HIGH);
    }
  }
  
  // Handle sweeping
  if (isSweeping) {
    currentPos += sweepDirection;
    
    if (currentPos >= upperLimit) {
      currentPos = upperLimit;
      sweepDirection = -1;
    } else if (currentPos <= lowLimit) {
      currentPos = lowLimit;
      sweepDirection = 1;
    }
    
    myServo.write(currentPos);
    updateLCD();
    delay(sweepSpeed);
  }
  
  // Handle serial commands
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    // Switch from welcome to angle display on first command
    if (showingWelcome) {
      showingWelcome = false;
      updateLCD();
    }
    
    char cmd = input.charAt(0);
    
    // Check if it's a number
    if (isDigit(cmd)) {
      int pos = input.toInt();
      if (pos >= 0 && pos <= 180) {
        isSweeping = false;
        currentPos = pos;
        myServo.write(currentPos);
        updateLCD();
        Serial.print("Moved to position: ");
        Serial.println(currentPos);
      } else {
        Serial.println("Position must be 0-180");
      }
    }
    // Handle letter commands
    else {
      switch (cmd) {
        case 'h':
        case 'H':
          homePos = currentPos;
          Serial.print("HOME set to: ");
          Serial.println(homePos);
          break;
          
        case 'l':
        case 'L':
          lowLimit = currentPos;
          Serial.print("LOW limit set to: ");
          Serial.println(lowLimit);
          break;
          
        case 'u':
        case 'U':
          upperLimit = currentPos;
          Serial.print("UPPER limit set to: ");
          Serial.println(upperLimit);
          break;
          
        case 's':
        case 'S':
          isSweeping = true;
          sweepDirection = 1;
          Serial.println("Sweeping started");
          break;
          
        case 'x':
        case 'X':
          isSweeping = false;
          Serial.println("Sweeping stopped");
          break;
          
        case 'p':
        case 'P':
          printSettings();
          break;
          
        case 'g':
        case 'G':
          isSweeping = false;
          currentPos = homePos;
          myServo.write(currentPos);
          updateLCD();
          Serial.print("Moved to HOME: ");
          Serial.println(homePos);
          break;
          
        default:
          Serial.println("Unknown command");
      }
    }
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SERVO ANGLE:");
  lcd.setCursor(0, 1);
  lcd.print(currentPos);
}

void printSettings() {
  Serial.println("\n--- Current Settings ---");
  Serial.print("Current Position: ");
  Serial.println(currentPos);
  Serial.print("HOME Position: ");
  Serial.println(homePos);
  Serial.print("LOW Limit: ");
  Serial.println(lowLimit);
  Serial.print("UPPER Limit: ");
  Serial.println(upperLimit);
  Serial.print("Sweeping: ");
  Serial.println(isSweeping ? "YES" : "NO");
  Serial.println("------------------------\n");
}
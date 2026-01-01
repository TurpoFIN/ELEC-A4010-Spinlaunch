#include <Servo.h>

Servo ESC;

String inputString = "";         
bool stringComplete = false;
bool incomingString = false;
int parsedValue = 0;

int speed = 0;
int targetSpeed = 0;
int brakeSpeed = 5;
int brakeDelay = 50;

unsigned long previousRampMillis = 0;
const long rampInterval = 750; 

int launchPin = 10;
unsigned long launchDelay = 3000000; 
unsigned long launchDiff = 6000000;  
unsigned long t1 = 0;

bool launch = false;
bool hasReachedTargetSpeed = false;
bool solenoidFired = false; 

void setup() {
  Serial.begin(9600);
  Serial.println("=== ESC Launch System ===");
  
  pinMode(launchPin, OUTPUT);
  digitalWrite(launchPin, LOW);

  ESC.attach(9); 
  
  Serial.println("Arming ESC (Sending Neutral)...");
  ESC.write(90); 
  delay(3000); 
  Serial.println("ESC armed. READY.");
}

void loop() {
  if (stringComplete) {
    launch = false;
    hasReachedTargetSpeed = false;
    solenoidFired = false;
    digitalWrite(launchPin, LOW);
    
    stringComplete = false;
    
    char charBuf[inputString.length() + 1];
    inputString.toCharArray(charBuf, sizeof(charBuf));

    char *endPtr;
    parsedValue = strtol(charBuf, &endPtr, 10);

    if (*endPtr == '\0' || *endPtr == '\n' || *endPtr == '\r') {
      if (parsedValue > 8 ) {
        incomingString = false;
        Serial.print("Target received: ");
        Serial.println(parsedValue);

        targetSpeed = parsedValue;
        speed = 7;
        launch = true; 
      } else {
        Serial.println("Target too low. Please set target to at least 6.");
      }
      
    } else {
      if (inputString.charAt(0) == 's') {
        targetSpeed = 0;
        brake(targetSpeed);
        launch = false;
      } else if (inputString.charAt(0) == '-') {
        targetSpeed -= 10;
      } else if (inputString.charAt(0) == '+') {
        targetSpeed += 10;
      }
    }

    targetSpeed = clamp(targetSpeed, 0, 180);
    inputString = "";
  } 
  
  else {
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousRampMillis >= rampInterval) {
      previousRampMillis = currentMillis;

      if (speed < targetSpeed) {
        speed++;
        Serial.print("Ramping up: "); Serial.println(speed);
        
        if (speed == targetSpeed) {
           hasReachedTargetSpeed = true;
           t1 = micros(); 
           Serial.println("Target reached. Waiting for launch delay...");
        }

      } else if (speed > targetSpeed) {
        speed--;
        Serial.print("Ramping down: "); Serial.println(speed);
        
        if (launch) {
           launch = false;
           hasReachedTargetSpeed = false;
           digitalWrite(launchPin, LOW);
           Serial.println("Launch Aborted (Speed dropped).");
        }
      }
      
      int escSignal = map(speed, 0, 180, 90, 180);
      ESC.write(escSignal);
    }

    if (launch && hasReachedTargetSpeed) {
      unsigned long currentTime = micros();
      unsigned long elapsed = currentTime - t1; 

      if (!solenoidFired && elapsed >= launchDelay) {
        digitalWrite(launchPin, HIGH);
        solenoidFired = true;
        Serial.println("LAUNCH!");
      } 
      
      else if (elapsed >= (launchDelay + launchDiff)) {
        digitalWrite(launchPin, LOW);
        Serial.println("Sequence Complete. Shutting down...");
        
        launch = false;          
        hasReachedTargetSpeed = false;
        targetSpeed = 0;            
        brake(0);                  
      }
    }
  }
}

int clamp(int value, int minVal, int maxVal) {
  if (value < minVal) return minVal;
  if (value > maxVal) return maxVal;
  return value;
}

void brake(int targetSpeed) {
  Serial.println("Braking...");
  for(speed -= brakeSpeed; speed > brakeSpeed; speed -= brakeSpeed) {
    speed = clamp(speed, 0, 180);
    
    int escSignal = map(speed, 0, 180, 90, 180);
    ESC.write(escSignal);
    
    delay(brakeDelay);
  }

  ESC.write(90);
  speed = 0;
  Serial.println("Stopped.");
}

void serialEvent() {
  while (Serial.available()) {
    incomingString = true;
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

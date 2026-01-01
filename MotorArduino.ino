// Tämä koodi ohjaa moottoria ja määrittää laikaisuikkunan
// Se lukee komentoja sarjaportista ja kiihdyttää moottorin haluttuun vauhtiin.

#include <Servo.h>

Servo ESC;

// Muuttujat sarjaportin lukemiseen ja komentojen käsittelyyn.
String inputString = "";         
bool stringComplete = false;
bool incomingString = false;
int parsedValue = 0;

// Moottorin nopeuden hallinta (0-180 asteikolla).
int speed = 0;
int targetSpeed = 0;
int brakeSpeed = 5;
int brakeDelay = 50;

// Säädetään, kuinka nopeasti moottorin vauhti nousee tai laskee.
unsigned long previousRampMillis = 0;
const long rampInterval = 750; 

// Laukaisuun liittyvät pinnit ja aikaviiveet.
int launchPin = 10;
unsigned long launchDelay = 3000000; 
unsigned long launchDiff = 6000000;  
unsigned long t1 = 0;

// Pidetään kirjaa siitä, missä vaiheessa laukaisuprosessi menee.
bool launch = false;
bool hasReachedTargetSpeed = false;
bool solenoidFired = false; 

// Alustetaan laitteisto ja laitetaan ESC valmiustilaan.
void setup() {
  Serial.begin(9600);
  Serial.println("=== ESC Launch System ===");
  
  pinMode(launchPin, OUTPUT);
  digitalWrite(launchPin, LOW);

  ESC.attach(9); 
  
  // ESC pitää yleensä "armata" lähettämällä sille neutraali signaali alussa.
  Serial.println("Arming ESC (Sending Neutral)...");
  ESC.write(90); 
  delay(3000); 
  Serial.println("ESC armed. READY.");
}

// Pääsilmukka, jossa käsitellään komennot ja ohjataan moottoria.
void loop() {
  // Jos sarjaportista tuli uusi komento, se käsitellään tässä.
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

    // Tarkistetaan, oliko kyseessä numero (uusi tavoitenopeus) vai jokin muu komento.
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
      // Käsitellään 's' (pysäytys) ja '+' / '-' säädöt.
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

    // Pidetään tavoitenopeus sallituissa rajoissa.
    targetSpeed = clamp(targetSpeed, 0, 180);
    inputString = "";
  } 
  
  else {
    unsigned long currentMillis = millis();
    
    // Kiihdytetään tai jarrutetaan moottoria vähitellen kohti tavoitetta.
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
      
      // Muunnetaan nopeusarvo ESC:n ymmärtämäksi signaaliksi.
      int escSignal = map(speed, 0, 180, 90, 180);
      ESC.write(escSignal);
    }

    // Jos ollaan tavoitenopeudessa, hoidetaan laukaisu oikeaan aikaan.
    if (launch && hasReachedTargetSpeed) {
      unsigned long currentTime = micros();
      unsigned long elapsed = currentTime - t1; 

      // Aktuoidaan solenoidi laukaisun hetkellä.
      if (!solenoidFired && elapsed >= launchDelay) {
        digitalWrite(launchPin, HIGH);
        solenoidFired = true;
        Serial.println("LAUNCH!");
      } 
      
      // Kun laukaisu on ohi, ajetaan järjestelmä alas.
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

// Apufunktio, joka pitää luvun annettujen rajojen sisällä.
int clamp(int value, int minVal, int maxVal) {
  if (value < minVal) return minVal;
  if (value > maxVal) return maxVal;
  return value;
}

// Hallittu jarrutus, jotta moottori ei jarruta liian kovaan.
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

// Luetaan sarjaportista tulevat merkit talteen.
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

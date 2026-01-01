// Ohjelmalla vastaanotetaan käyttäjän antama nopeus ja lähetetään se toiselle Arduinolle: LCD-näyttöä, nappia ja RPM-asetuksia.

#include <LiquidCrystal.h>
 
// Määritetään pinnit, joihin LCD-näyttö on kytketty.
LiquidCrystal lcd(11, 10, 5, 4, 3, 2);
 
const int buttonPin = 7;
bool buttonPressed = false;
bool lastButtonState = HIGH;
 
// Potentiometri tai sensori, jolla säädetään nopeutta.
const int sensorPin = A0;
int sensorValue = 0;
int rpmValue = 0;

// Tekstit, jotka rullaavat näytöllä laukaisun jälkeen.
String line1 = "****Throwing****";
String line2 = "****Satellite***";
int lcdCols = 16;
int scrollIndex = 0; 
 
// Alustetaan pinnit ja näyttö.
void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(sensorPin, INPUT);
 
  lcd.begin(16, 2);
  Serial.begin(9600);
 
  lcd.print("Starting System");
  delay(1000);
  lcd.clear();
  lcd.print("System On");
  delay(1500);
  lcd.clear();
}
 
// Lasketaan sekunteja alaspäin ennen laukaisua.
void countDown(int wantedValue) {
  for (int i = 10; i >= 0; i--) {
    lcd.setCursor(0, 0);
    lcd.print("Countdown: ");
    lcd.print(i);
    lcd.print("   ");
    delay(1000);
  }
 
  lcd.setCursor(0, 0);
  lcd.print("Countdown Done!   ");
  lcd.setCursor(0, 1);
  lcd.print("Sendoff: ");
  lcd.setCursor(10, 1);
  lcd.print(wantedValue);
 
  // Lähetetään lopullinen arvo toiselle Arduinolle sarjaportin kautta.
  Serial.println(wantedValue);
 
  delay(5000);
  lcd.clear();
}
 
// Rullataan tekstiä näytöllä, jotta se näyttää hienommalta.
void scrollText() {
  String visible1 = "";
  String visible2 = "";
 
  for (int j = 0; j < lcdCols; j++) {
    visible1 += line1[(scrollIndex + j) % line1.length()];
    visible2 += line2[(scrollIndex + j) % line2.length()];
  }
 
  lcd.setCursor(0, 0);
  lcd.print(visible1);
  lcd.setCursor(0, 1);
  lcd.print(visible2);
 
  scrollIndex--;
  if (scrollIndex < 0) scrollIndex = line1.length() - 1;
 
  delay(300);
}
 
// Tarkistetaan nappeja ja hoidetaan RPM-arvon lähettäminen, kun nappia painetaan.
void checkButtonAndSendRPM() {
  bool currentButtonState = digitalRead(buttonPin);
 
  // Katsotaan, onko nappia juuri painettu (reunan tunnistus).
  if (!buttonPressed && lastButtonState == HIGH && currentButtonState == LOW) {
    buttonPressed = true;
    sensorValue = analogRead(sensorPin);
    // Muunnetaan sensorin arvo RPM-lukemaksi.
    rpmValue = map(sensorValue, 0, 1023, 0, 1200);
    countDown(rpmValue);
  }
 
  lastButtonState = currentButtonState;
}
 
// Pääsilmukka päivittää näytön lukemia ja tarkkailee nappia.
void loop() {
  if (!buttonPressed) {
    sensorValue = analogRead(sensorPin);
    rpmValue = map(sensorValue, 0, 1023, 0, 1200);
 
    // Näytetään säädettävä arvo ja vastaava RPM ruudulla.
    lcd.setCursor(0, 0);
    lcd.print("Wanted h:");
    lcd.setCursor(11, 0);
    lcd.print(sensorValue);
    lcd.print("   ");
 
    lcd.setCursor(0, 1);
    lcd.print("RPM: ");
    lcd.setCursor(5, 1);
    lcd.print(rpmValue);
    lcd.print("   ");
  }
 
  checkButtonAndSendRPM();
 
  // Jos laukaisu on tehty, rullataan tekstiä.
  if (buttonPressed) scrollText();
 
  delay(50);
}

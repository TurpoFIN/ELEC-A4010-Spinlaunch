#include <LiquidCrystal.h>
 
LiquidCrystal lcd(11, 10, 5, 4, 3, 2);
 
const int buttonPin = 7;
bool buttonPressed = false;
bool lastButtonState = HIGH;
 
const int sensorPin = A0;
int sensorValue = 0;
int rpmValue = 0;

String line1 = "****Throwing****";
String line2 = "****Satellite***";
int lcdCols = 16;
int scrollIndex = 0; 
 
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
 
// Countdown function
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
 
  Serial.println(wantedValue);
 
  delay(5000);
  lcd.clear();
}
 
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
 
void checkButtonAndSendRPM() {
  bool currentButtonState = digitalRead(buttonPin);
 
  if (!buttonPressed && lastButtonState == HIGH && currentButtonState == LOW) {
    buttonPressed = true;
    sensorValue = analogRead(sensorPin);
    rpmValue = map(sensorValue, 0, 1023, 0, 1200);
    countDown(rpmValue);
  }
 
  lastButtonState = currentButtonState;
}
 
void loop() {
  if (!buttonPressed) {
    sensorValue = analogRead(sensorPin);
    rpmValue = map(sensorValue, 0, 1023, 0, 1200);
 
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
 
  if (buttonPressed) scrollText();
 
  delay(50);
}
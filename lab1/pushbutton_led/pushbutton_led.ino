#define LED 5
#define BUTTON 6
void setup() {
  // put your setup code here, to run once:
  // pinMode(LED, INPUT_PULLDOWN);
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  if(digitalRead(BUTTON)) {
    digitalWrite(LED, HIGH);
    Serial.print("button pressed\n");
    delay(100);
  } else {
    digitalWrite(LED, LOW);
    Serial.print("button not pressed\n");
    delay(100);
  }
  // digitalWrite(LED, HIGH);
  
}

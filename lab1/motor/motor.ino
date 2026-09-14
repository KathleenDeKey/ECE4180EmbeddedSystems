#define POT 4
#define MOTOR 5
#define IN1 19
#define IN2 18

int value;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  ledcAttach(MOTOR, 5000, 12);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  value = 0;

}

void loop() {
  uint16_t potValue = analogReadMilliVolts(POT);
  // Serial.println(potValue);
  
  if (potValue <= 2350 ){
    Serial.println("increasing negative direction");
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    value = (2350 - potValue) * 2;
  }
  else if ((potValue > 2350) && (potValue < 2450)) {
    Serial.println("stopping motor");
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    value = 0;
  }
  else if (potValue >= 2450) {
    Serial.println("increasing positive direction");
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
     value = (potValue - 2350) * 2;
  } 

  if (value > 4096) {
    value = 4096;
  } else if (value < 0) {
    value = 0;
  }

  Serial.println(value);
  ledcWrite(MOTOR, value);
  delay(10);
}

#define RED_OUT 36
#define GREEN_OUT 37
#define BLUE_OUT 38

#define BUTTON1 45
#define BUTTON2 48

int state;  // 0 = red, 1 = green, 2 = blue, 3 = yellow 

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(BUTTON1, INPUT_PULLDOWN);
  pinMode(BUTTON2, INPUT_PULLDOWN);

  pinMode(RED_OUT, OUTPUT);
  pinMode(GREEN_OUT, OUTPUT);
  pinMode(BLUE_OUT, OUTPUT);

  state = 0;
}

void loop() {
  if(digitalRead(BUTTON1)) {
    state = state+1;
    Serial.print("positive button pressed\n");
  }
  else if(digitalRead(BUTTON2)) {
    state = state-1;
    Serial.print("negative button pressed\n");
  }

  // display correct state
  if(state > 3) {
    state = state - 4;
  }
  else if (state < 0) {
    state = state + 4;
  }
  else if (state == 0) {
    Serial.print("RED\n");
    digitalWrite(RED_OUT, HIGH);
    digitalWrite(GREEN_OUT, LOW);
    digitalWrite(BLUE_OUT, LOW);
  }
  else if (state == 1) {
    Serial.print("GREEN\n");
    digitalWrite(RED_OUT, LOW);
    digitalWrite(GREEN_OUT, HIGH);
    digitalWrite(BLUE_OUT, LOW);
  }
  else if (state == 2) {
    Serial.print("BLUE\n");
    digitalWrite(RED_OUT, LOW);
    digitalWrite(GREEN_OUT, LOW);
    digitalWrite(BLUE_OUT, HIGH);
  }
  else if(state == 3) {
    Serial.print("YELLOW\n");
    digitalWrite(RED_OUT, HIGH);
    digitalWrite(GREEN_OUT, HIGH);
    digitalWrite(BLUE_OUT, LOW);
  }
  else {
    Serial.print("unrecognized state");
  }
  delay(100);
}

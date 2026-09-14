#define UP 1 
#define DOWN 2
#define LEFT 3
#define RIGHT 6
#define CENTER 7


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(UP, INPUT_PULLDOWN);
  pinMode(DOWN, INPUT_PULLDOWN);
  pinMode(LEFT, INPUT_PULLDOWN);
  pinMode(RIGHT, INPUT_PULLDOWN);
  pinMode(CENTER, INPUT_PULLDOWN);
}

void loop() {
  if (!digitalRead(UP) && !digitalRead(RIGHT)) {
    Serial.print("top right\n");
  } 
  else if (!digitalRead(UP) && !digitalRead(LEFT)) {
    Serial.print("top left\n");
  } 
  else if (!digitalRead(DOWN) && !digitalRead(RIGHT)) {
    Serial.print("bottom right\n");
  }
  else if (!digitalRead(DOWN) && !digitalRead(LEFT)) {
    Serial.print("bottom left\n");
  }
  else if (!digitalRead(UP)) {
    Serial.print("up\n");
  } 
  else if (!digitalRead(DOWN)) {
    Serial.print("down\n");
  }
  else if (!digitalRead(LEFT)) {
    Serial.print("left\n");
  } 
  else if (!digitalRead(RIGHT)) {
    Serial.print("right\n");
  }
  else if (!digitalRead(CENTER)) {
    Serial.print("center\n");
  }
  else {
    Serial.print("nothing\n");
  }
  delay(300);
}

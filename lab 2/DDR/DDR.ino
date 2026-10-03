#include <Adafruit_CH9328.h>
Adafruit_CH9328 keyboard;

HardwareSerial MySerial(1);
// SoftwareSerial Serial1(3,2);

#define CH9328_TX_PIN 18   // ESP32-S3 TX -> CH9328 RX
#define CH9328_RX_PIN 17

#define UP 12 
#define DOWN 11
#define LEFT 13
#define RIGHT 14
#define CENTER 10


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(CENTER, INPUT_PULLUP);

  // Serial1.begin(9600);
  // keyboard.begin(&Serial1);
  MySerial.begin(9600, SERIAL_8N1, CH9328_RX_PIN, CH9328_TX_PIN);  // keyboard.begin(&MySerial);
  keyboard.begin(&MySerial);
}

void loop() {
  if (!digitalRead(UP)) {
    Serial.print("up\n");
    byte keys[6] = {KEY_W, 0, 0, 0, 0, 0};
    byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};
    keyboard.sendKeyPress(keys, 0);
    keyboard.sendKeyPress(noKeysPressed,0);
  } 
  else if (!digitalRead(DOWN)) {
    Serial.print("down\n");
    keyboard.typeString("s");
  }
  else if (!digitalRead(LEFT)) {
    Serial.print("left\n");
    keyboard.typeString("a");
  } 
  else if (!digitalRead(RIGHT)) {
    Serial.print("right\n");
    byte keys[6] = {KEY_RIGHT, 0, 0, 0, 0, 0};
    byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};
    keyboard.sendKeyPress(keys, 0);
    keyboard.sendKeyPress(noKeysPressed, 0);
  }
  else if (!digitalRead(CENTER)) {
    Serial.print("center\n");

    byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};
    byte sequence[] = {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, KEY_B, KEY_A};

    for (int i = 0; i < sizeof(sequence); i++) {
      byte keys[6] = {sequence[i], 0, 0, 0, 0, 0};
      keyboard.sendKeyPress(keys, 0);
      keyboard.sendKeyPress(noKeysPressed, 0);
      delay(50); // small gap so each press registers as distinct
  }
}
  else {
    Serial.print("nothing\n");
  }
  delay(10);
}
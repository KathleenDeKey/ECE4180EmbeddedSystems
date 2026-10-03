#include <Wire.h>

#define FORWARD  0x01
#define BACKWARD 0x02
#define LEFT     0x04
#define RIGHT    0x08
#define UP       0x10
#define DOWN     0x20

#define RED_PIN   15
#define GREEN_PIN 16
#define BLUE_PIN  17

// Set true if your LED is COMMON ANODE (needs inverted PWM values)
#define COMMON_ANODE false

volatile uint8_t lastCode = 0;
volatile bool newData = false;

void setup() {
  Serial.begin(115200);
  Wire.setPins(11, 12);
  Wire.begin(0x08);
  Wire.onReceive(receiveEvent);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setColor(0, 0, 0); // off at start
}

void loop() {
  if (newData) {
    newData = false;
    uint8_t code = lastCode;

    // Priority order matters if multiple bits are set at once.
    // Adjust the order/logic below to fit how you want combos handled.
    if (code & FORWARD)       { Serial.println("Forward");  setColor(0, 255, 0); }   // green
    else if (code & BACKWARD) { Serial.println("Backward"); setColor(255, 0, 0); }   // red
    else if (code & LEFT)     { Serial.println("Left");     setColor(0, 0, 255); }   // blue
    else if (code & RIGHT)    { Serial.println("Right");    setColor(255, 255, 0); } // yellow
    else if (code & UP)       { Serial.println("Up");       setColor(0, 255, 255); } // cyan
    else if (code & DOWN)     { Serial.println("Down");     setColor(255, 0, 255); } // magenta
    else                       setColor(0, 0, 0); // no bits set -> off
  }
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  if (COMMON_ANODE) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void receiveEvent(int numBytes) {
  while (Wire.available()) {
    lastCode = Wire.read();
    newData = true;
  }
}
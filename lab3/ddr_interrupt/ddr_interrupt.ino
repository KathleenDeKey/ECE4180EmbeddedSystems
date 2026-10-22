#include <Adafruit_CH9328.h>
Adafruit_CH9328 keyboard;

HardwareSerial MySerial(1);

#define CH9328_TX_PIN 18   // ESP32-S3 TX -> CH9328 RX
#define CH9328_RX_PIN 17

#define UP     12
#define DOWN   11
#define LEFT   13
#define RIGHT  14
#define CENTER 10

#define DEBOUNCE_MS 50

struct Button {
  uint8_t pin;
  byte key;                 // 0 = special (konami)
  const char* name;
  volatile bool flag;
  volatile uint32_t last;
};

Button buttons[] = {
  {UP,     KEY_UP,    "up",     false, 0},
  {DOWN,   KEY_DOWN,  "down",   false, 0},
  {LEFT,   KEY_LEFT,  "left",   false, 0},
  {RIGHT,  KEY_RIGHT, "right",  false, 0},
  {CENTER, 0,         "center", false, 0},
};
const size_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

// One ISR for every button: set the flag only, nothing else
void IRAM_ATTR isr(void* arg) {
  Button* b = (Button*)arg;
  uint32_t now = millis();
  if (now - b->last > DEBOUNCE_MS) {
    b->flag = true;
    b->last = now;
  }
}

void tapKey(byte key) {
  byte keys[6] = {key, 0, 0, 0, 0, 0};
  byte none[6] = {0, 0, 0, 0, 0, 0};
  keyboard.sendKeyPress(keys, 0);
  keyboard.sendKeyPress(none, 0);
}

void konami_code() {
  // const byte sequence[] = {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN,
  //                            KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT,
  //                            KEY_B, KEY_A};
  // byte keys[6] = {KEY_UP, 0, KEY_UP, 0, KEY_DOWN, 0};
  byte UP_keys[6] = {KEY_UP, 0, 0, 0, 0, 0};
  byte DOWN_keys[6] = {KEY_DOWN, 0,0,0,0,0};
  byte LEFT_keys[6] = {KEY_LEFT, 0,0, 0,0,0};
  byte RIGHT_keys[6] = {KEY_RIGHT, 0, 0, 0, 0, 0};
  byte A_keys[6] = {KEY_A, 0, 0,0, 0, 0};
  byte B_keys[6] = {KEY_B, 0, 0, 0, 0, 0};
  byte none[6] = {0, 0, 0, 0, 0, 0};
  keyboard.sendKeyPress(UP_keys,0);
  keyboard.sendKeyPress(none, 0);
  keyboard.sendKeyPress(UP_keys, 0);
  keyboard.sendKeyPress(DOWN_keys, 0);
  keyboard.sendKeyPress(none, 0);
  keyboard.sendKeyPress(DOWN_keys, 0);
  keyboard.sendKeyPress(LEFT_keys, 0);
  keyboard.sendKeyPress(RIGHT_keys, 0);
  keyboard.sendKeyPress(LEFT_keys, 0);
  keyboard.sendKeyPress(RIGHT_keys, 0);
  keyboard.sendKeyPress(B_keys, 0);
  keyboard.sendKeyPress(A_keys, 0);
  keyboard.sendKeyPress(none, 0);
  // keyboard.sendKeyPress(keys, 0);
  // keyboard.sendKeyPress(none, 0);
  // byte keys2[6] = {KEY_DOWN, 0, KEY_LEFT, 0, KEY_RIGHT, 0};
  // keyboard.sendKeyPress(keys2, 0);
  // keyboard.sendKeyPress(none, 0);
  // byte keys3[6] =  {KEY_LEFT, 0, KEY_RIGHT,0 , KEY_B, 0};
  // keyboard.sendKeyPress(keys3, 0);
  // keyboard.sendKeyPress(none, 0);
  // byte keys4[6] =  {KEY_A, 0, 0,0 , 0, 0};
  // keyboard.sendKeyPress(keys4, 0);
  // keyboard.sendKeyPress(none, 0);
  // byte keys2[6] = {0, 0, KEY_B, KEY_A, 0, 0};
  // keyboard.sendKeyPress(keys2, 0);
  // keyboard.sendKeyPress(none, 0);
}

void setup() {
  Serial.begin(115200);

  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(buttons[i].pin), isr, &buttons[i], FALLING);
  }

  MySerial.begin(9600, SERIAL_8N1, CH9328_RX_PIN, CH9328_TX_PIN);
  keyboard.begin(&MySerial);
}

void loop() {
  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    Button& b = buttons[i];
    if (b.flag) {
      b.flag = false;
      Serial.println(b.name);
      if (b.key) tapKey(b.key);
      else       konami_code();
    }
  }
}
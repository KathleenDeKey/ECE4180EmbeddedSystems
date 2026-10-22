#include <Adafruit_CH9328.h>
#include <Arduino.h>
Adafruit_CH9328 keyboard;

HardwareSerial MySerial(1);
// SoftwareSerial Serial1(3,2);

#define CH9328_TX_PIN 17   // ESP32-S3 TX -> CH9328 RX
#define CH9328_RX_PIN 6

#define UP 12 
#define DOWN 11
#define LEFT 13
#define RIGHT 14
#define CENTER 18

const uint8_t BOARD_NUMBER = 24; // Replace with your assigned Board Number (0 to 175)
uint16_t lfsr_state;

const unsigned long BPM = 120;
const unsigned long BEAT_MS = 60000UL / BPM; 

unsigned long stepCount = 0;
unsigned long nextBeat = 0;

unsigned long beatNum = 0;

void prng_init(uint8_t board_id) {
    lfsr_state = ((uint16_t)board_id << 8) | (board_id ^ 0x5A);
    if (lfsr_state == 0) lfsr_state = 0xACE1;
}

uint8_t prng_step() {
    uint8_t lsb = lfsr_state & 1;
    lfsr_state >>= 1;
    if (lsb == 1) {
        lfsr_state ^= 0xB400; // Feedback polynomial taps
    }
    return (lfsr_state % 4); // 0: LEFT, 1: DOWN, 2: UP, 3: RIGHT
}

const char* DIR_NAMES[4] = {"LEFT", "DOWN", "UP", "RIGHT"};
bool start = false;

void sendArrow(uint8_t dir) {
  byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};

  switch (dir) {
    case 0: { // LEFT
      Serial.print("left\n");
      byte keys[6] = {KEY_LEFT, 0, 0, 0, 0, 0};
      keyboard.sendKeyPress(keys, 0);
      keyboard.sendKeyPress(noKeysPressed, 0);
      break;
    }

    case 1: { // DOWN
      Serial.print("down\n");
      byte keys[6] = {KEY_DOWN, 0, 0, 0, 0, 0};
      keyboard.sendKeyPress(keys, 0);
      keyboard.sendKeyPress(noKeysPressed, 0);
      break;
    }

    case 2: { // UP
      Serial.print("up\n");
      byte keys[6] = {KEY_UP, 0, 0, 0, 0, 0};
      keyboard.sendKeyPress(keys, 0);
      keyboard.sendKeyPress(noKeysPressed, 0);
      break;
    }

    case 3: { // RIGHT
      Serial.print("right\n");
      byte keys[6] = {KEY_RIGHT, 0, 0, 0, 0, 0};
      keyboard.sendKeyPress(keys, 0);
      keyboard.sendKeyPress(noKeysPressed, 0);
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize the CH9328 link FIRST
  MySerial.begin(9600, SERIAL_8N1, CH9328_RX_PIN, CH9328_TX_PIN);  // keyboard.begin(&MySerial);
  keyboard.begin(&MySerial);

  Serial.println("===============================================");
  Serial.printf("ECE 4180: ESP32 PRNG Demo for Board #%d\n", BOARD_NUMBER);
  Serial.println("===============================================");

  // Preview the first 16 predictions
  prng_init(BOARD_NUMBER);
  Serial.println("First 16 predicted arrow directions:");
  for (int i = 0; i < 16; i++) {
    uint8_t dir = prng_step();
    Serial.printf("  Step #%02d -> %s (%d)\n", i + 1, DIR_NAMES[dir], dir);
  }

  // Reset the PRNG so playback starts at step 1 again
  prng_init(BOARD_NUMBER);

  pinMode(CENTER, INPUT_PULLUP);

  // Give yourself time to click into the game window
  nextBeat = millis() + 3000;
}

void loop() {
  if (!digitalRead(CENTER)) {
    Serial.print("center\n");
    start = true;
  }
  if (start) {
    unsigned long now = millis();

    if (now >= nextBeat) {
        if (beatNum == 3 || beatNum == 4) {
            uint8_t dir = prng_step();   // predicted next arrow
            sendArrow(dir);
        }

    stepCount++;
    Serial.print("Step #");
    Serial.println(stepCount);

    nextBeat += BEAT_MS;         // fixed schedule, no drift
    beatNum = (beatNum + 1)%8;
    }
  }
   
}
//   if (!digitalRead(UP)) {
//     Serial.print("up\n");
//     byte keys[6] = {KEY_W, 0, 0, 0, 0, 0};
//     byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};
//     keyboard.sendKeyPress(keys, 0);
//     keyboard.sendKeyPress(noKeysPressed,0);
//   } 
//   else if (!digitalRead(DOWN)) {
//     Serial.print("down\n");
//     keyboard.typeString("s");
//   }
//   else if (!digitalRead(LEFT)) {
//     Serial.print("left\n");
//     keyboard.typeString("a");
//   } 
//   else if (!digitalRead(RIGHT)) {
//     Serial.print("right\n");
//     byte keys[6] = {KEY_RIGHT, 0, 0, 0, 0, 0};
//     byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};
//     keyboard.sendKeyPress(keys, 0);
//     keyboard.sendKeyPress(noKeysPressed, 0);
//   }
//   else if (!digitalRead(CENTER)) {
//     Serial.print("center\n");

//     byte noKeysPressed[6] = {0, 0, 0, 0, 0, 0};
//     byte sequence[] = {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, KEY_B, KEY_A};

//     for (int i = 0; i < sizeof(sequence); i++) {
//       byte keys[6] = {sequence[i], 0, 0, 0, 0, 0};
//       keyboard.sendKeyPress(keys, 0);
//       keyboard.sendKeyPress(noKeysPressed, 0);
//       delay(50); // small gap so each press registers as distinct
//   }
// }
//   else {
//     Serial.print("nothing\n");
//   }
//   delay(10);
// }

#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// ---- I2C pins for the MPR121 (unchanged) ----
#define SDA_PIN 23
#define SCL_PIN 22

// ---- UART pins to the receiver board (change if needed) ----
#define UART_TX 16   // goes to receiver GPIO 18 (its RX)
#define UART_RX 17   // unused, but required by begin()

#define CLEAR_PAD 10   // pad 10 = clear the current entry
#define CODE_LEN  4

Adafruit_MPR121 cap = Adafruit_MPR121();

uint16_t lasttouched = 0;
uint16_t currtouched = 0;

char code[CODE_LEN + 1] = {0};  // 4 digits + null terminator
uint8_t codeIndex = 0;

void sendCodeUART() {
  Serial1.println(code);   // sends "1234\r\n"
  Serial.print("Sent over UART: ");
  Serial.println(code);
}

void resetCode() {
  codeIndex = 0;
  memset(code, 0, sizeof(code));
}

void handlePress(uint8_t pad) {
  if (pad == CLEAR_PAD) {
    resetCode();
    Serial.println("Entry cleared");
    return;
  }
  if (pad > 9) return;  // ignore pad 11

  code[codeIndex++] = '0' + pad;
  Serial.print("Digit "); Serial.print(codeIndex);
  Serial.print(": "); Serial.println(pad);

  if (codeIndex == CODE_LEN) {
    Serial.print("Code entered: "); Serial.println(code);
    sendCodeUART();
    resetCode();
  }
}

void setup() {
  Serial.begin(9600);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) delay(10);
  delay(500);

  // UART to the receiver: 9600 baud, 8N1 (matches your receiver sketch)
  Serial1.begin(9600, SERIAL_8N1, UART_RX, UART_TX);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!cap.begin(0x5A, &Wire)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1) delay(10);
  }
  cap.setAutoconfig(true);

  Serial.println("Ready. Enter a 4-digit code (pad 10 = clear).");
}

void loop() {
  currtouched = cap.touched();

  for (uint8_t i = 0; i < 12; i++) {
    // Only act on a new press, not on hold or release
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {
      handlePress(i);
    }
  }

  lasttouched = currtouched;
  delay(10);
}
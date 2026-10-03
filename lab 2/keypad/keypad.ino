#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// ---- CHANGE THESE to the GPIOs you actually wired ----
#define SDA_PIN 8
#define SCL_PIN 9

// Set to true for detailed filtered/baseline data
#define DEBUG_DATA false

Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

void setup() {
  Serial.begin(115200);

  // Wait up to 3 seconds for the serial monitor, but don't hang forever
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }
  delay(500);

  Serial.println("Adafruit MPR121 Capacitive Touch sensor test");

  // This was missing from the original: tell the ESP32-S3 which pins to use
  Wire.begin(SDA_PIN, SCL_PIN);

  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A, &Wire)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1) delay(10);
  }
  Serial.println("MPR121 found!");

  // This is generally recommended since it seems to work well for most setups.
  // Can remove if wanting to manually configure touch channels (CDC and CDT).
  Serial.println("Running auto configuration.");
  cap.setAutoconfig(true);

  Serial.println("Initialization complete.");
}

void loop() {
  // Get the currently touched pads
  currtouched = cap.touched();

  for (uint8_t i = 0; i < 12; i++) {
    // if it *is* touched and *wasn't* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {
      Serial.print(i); Serial.println(" touched");
    }
    // if it *was* touched and now *isn't*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i))) {
      Serial.print(i); Serial.println(" released");
    }
  }

  // reset our state
  lasttouched = currtouched;

#if DEBUG_DATA
  // debugging info
  Serial.print("\t\t\t\t\t\t\t\t\t\t\t\t\t 0x"); Serial.println(cap.touched(), HEX);
  Serial.print("Filt: ");
  for (uint8_t i = 0; i < 12; i++) {
    Serial.print(cap.filteredData(i)); Serial.print("\t");
  }
  Serial.println();
  Serial.print("Base: ");
  for (uint8_t i = 0; i < 12; i++) {
    Serial.print(cap.baselineData(i)); Serial.print("\t");
  }
  Serial.println();
  delay(100);
#else
  delay(10);
#endif
}
#include <Adafruit_NeoPixel.h>

#define LED_PIN     8     // Data pin connected to onboard RGB LED for ESP32-C6
// #define LED_PIN    38  // Data pin connected to onboard RGB LED for ESP32-S3
#define NUM_LEDS    1     // Only one LED on board

Adafruit_NeoPixel pixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pixel.begin();           // Initialize NeoPixel
  pixel.setBrightness(20); // Make it less bright!
}

void loop() {
  pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Display Red
  pixel.show();
  delay(500);

  pixel.setPixelColor(0, pixel.Color(0, 0, 0));   // Off
  pixel.show();
  delay(500);
}


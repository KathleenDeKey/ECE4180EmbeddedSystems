#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define CS_PIN 38
#define RST_PIN 40
#define DC_PIN 39

Adafruit_ST7789 tft = Adafruit_ST7789(CS_PIN, DC_PIN, RST_PIN);

#define UP 3
#define DOWN 16
#define LEFT 17
#define RIGHT 15
#define CENTER 18

int current_x = 120;
int current_y = 120;
int radius = 10;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(CENTER, INPUT_PULLUP);

  tft.init(240, 240);
  tft.fillScreen(ST77XX_BLACK);
  tft.fillCircle(current_x, current_y, radius, ST77XX_BLUE);
}

void loop() {
  if (!digitalRead(UP)) {
    Serial.print("up\n");
    current_y += 2;
    if (current_y > 240) {
      current_x = 240;
    }
  } 
  if (!digitalRead(DOWN)) {
    Serial.print("down\n");
    current_y -= 2;
    if (current_y < 5) {
      current_y = 5;
    }
  }
  if (!digitalRead(LEFT)) {
    Serial.print("left\n"); 
    current_x -= 2;
    if (current_x < 5) {
      current_x = 5;
    }
  } 
  if (!digitalRead(RIGHT)) {
    Serial.print("right\n");
    current_x += 2;
    if (current_x > 240) {
      current_x = 240;
    }
  }
  else {
    Serial.print("nothing\n");
  }
  tft.fillScreen(ST77XX_BLACK);
  tft.fillCircle(current_x, current_y, radius, ST77XX_BLUE);
  delay(10);
}

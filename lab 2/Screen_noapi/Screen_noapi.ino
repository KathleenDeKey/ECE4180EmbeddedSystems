#include <SPI.h>

// ---------- Display pins ----------
#define CS_PIN  38
#define RST_PIN 40
#define DC_PIN  39
// SCK/MOSI use the board's default SPI pins. To use other pins, call
// SPI.begin(SCK, MISO, MOSI, CS) in setup().

// ---------- Buttons ----------
#define UP     8
#define DOWN   16
#define LEFT   17
#define RIGHT  15
#define CENTER 18

#define SCREEN_W 240
#define SCREEN_H 240

// RGB565 colors
#define COLOR_BLACK 0x0000
#define COLOR_BLUE  0x001F

int current_x = 120;
int current_y = 120;
int radius    = 10;

// ---------- Low-level display functions ----------
static const SPISettings tftSPI(40000000, MSBFIRST, SPI_MODE3);

void tftCommand(uint8_t cmd) {
  SPI.beginTransaction(tftSPI);
  digitalWrite(DC_PIN, LOW);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(cmd);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void tftData(uint8_t data) {
  SPI.beginTransaction(tftSPI);
  digitalWrite(DC_PIN, HIGH);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(data);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void tftData16(uint16_t d) {
  tftData(d >> 8);
  tftData(d & 0xFF);
}

void tftInit() {
  pinMode(CS_PIN, OUTPUT);
  pinMode(DC_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  // Hardware reset
  digitalWrite(RST_PIN, HIGH); delay(10);
  digitalWrite(RST_PIN, LOW);  delay(10);
  digitalWrite(RST_PIN, HIGH); delay(120);

  tftCommand(0x01); delay(150);          // SWRESET
  tftCommand(0x11); delay(120);          // SLPOUT
  tftCommand(0x3A); tftData(0x55);       // COLMOD: 16-bit color
  tftCommand(0x36); tftData(0x00);       // MADCTL: normal orientation
  tftCommand(0x21);                      // INVON (most ST7789 panels need this)
  tftCommand(0x13); delay(10);           // NORON
  tftCommand(0x29); delay(120);          // DISPON
}

void tftSetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  tftCommand(0x2A);                      // CASET
  tftData16(x0); tftData16(x1);
  tftCommand(0x2B);                      // RASET
  tftData16(y0); tftData16(y1);
  tftCommand(0x2C);                      // RAMWR
}

// Fill a rectangle with a solid color (clipped to the screen)
void tftFillRect(int x, int y, int w, int h, uint16_t color) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;

  tftSetWindow(x, y, x + w - 1, y + h - 1);

  static uint8_t buf[SCREEN_W * 2];
  for (int i = 0; i < w; i++) {
    buf[2 * i]     = color >> 8;
    buf[2 * i + 1] = color & 0xFF;
  }

  SPI.beginTransaction(tftSPI);
  digitalWrite(DC_PIN, HIGH);
  digitalWrite(CS_PIN, LOW);
  for (int row = 0; row < h; row++) {
    SPI.writeBytes(buf, w * 2);          // ESP32 SPI bulk write
  }
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void tftFillScreen(uint16_t color) {
  tftFillRect(0, 0, SCREEN_W, SCREEN_H, color);
}

// Filled circle drawn as horizontal spans
void tftFillCircle(int cx, int cy, int r, uint16_t color) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = (int)sqrtf((float)(r * r - dy * dy));
    tftFillRect(cx - dx, cy + dy, 2 * dx + 1, 1, color);
  }
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(CENTER, INPUT_PULLUP);

  SPI.begin();
  tftInit();
  tftFillScreen(COLOR_BLACK);
  tftFillCircle(current_x, current_y, radius, COLOR_BLUE);
}

void loop() {
  int old_x = current_x;
  int old_y = current_y;

  if (!digitalRead(UP))    current_y -= 2;   // screen y grows downward
  if (!digitalRead(DOWN))  current_y += 2;
  if (!digitalRead(LEFT))  current_x -= 2;
  if (!digitalRead(RIGHT)) current_x += 2;

  // Keep the whole circle on screen
  current_x = constrain(current_x, radius, SCREEN_W - 1 - radius);
  current_y = constrain(current_y, radius, SCREEN_H - 1 - radius);

  // Only redraw if it moved (no full-screen clear, so no flicker)
  if (current_x != old_x || current_y != old_y) {
    tftFillCircle(old_x, old_y, radius, COLOR_BLACK);
    tftFillCircle(current_x, current_y, radius, COLOR_BLUE);
  }

  delay(10);
}
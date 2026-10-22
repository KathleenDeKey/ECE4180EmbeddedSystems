#include <Wire.h>
#include "Adafruit_MPR121.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Arduino.h>
#include "ESP32_NOW_Serial.h"
#include "MacAddress.h"
#include "WiFi.h"
#include "esp_wifi.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// keyboard
#define SDA_PIN 6
#define SCL_PIN 9

// built-in RGB LED (GPIO8 on C6 DevKitC)
#ifdef RGB_BUILTIN
  #define LED RGB_BUILTIN
#else
  #define LED 8
#endif

// screen -- FILL IN YOUR REAL SPI PINS
#define CS_PIN  4
#define RST_PIN 10
#define DC_PIN  7
#define SCK_PIN  21   // <-- change to your wiring
#define MOSI_PIN 19   // <-- change to your wiring

#define CODE_LEN  4
#define CLEAR_PAD 11
#define PASSCODE  "1234"

#define STAR_SIZE 5
#define STAR_W    (6 * STAR_SIZE)
#define STAR_H    (8 * STAR_SIZE)
#define STAR_X0   ((240 - CODE_LEN * STAR_W) / 2)
#define STAR_Y    ((240 - STAR_H) / 2)

#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_WIFI_MODE WIFI_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA

const MacAddress peer_mac({0x94, 0xA9, 0x90, 0xDD, 0x31, 0x38});
ESP_NOW_Serial_Class NowSerial(peer_mac, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);

// Protocol bytes
#define MSG_DISARM 1
#define MSG_ARM    2
#define MSG_HEART  'H'

#define HEARTBEAT_MS   500
#define TIMEOUT_MS     2000
#define BLINK_TOTAL_MS 3000
#define BLINK_HALF_MS  250
#define DISARM_MS      30000

uint8_t  state = MSG_ARM;      // only ever ARM or DISARM
bool     connected = false;
uint32_t lastRx = 0, lastHeartbeat = 0, blinkUntil = 0, disarmStart = 0;

Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_ST7789 tft = Adafruit_ST7789(CS_PIN, DC_PIN, RST_PIN);

uint16_t lasttouched = 0, currtouched = 0;
char code[CODE_LEN + 1] = {0};
uint8_t codeIndex = 0;

void drawStar(uint8_t index) {
  tft.setTextSize(STAR_SIZE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(STAR_X0 + index * STAR_W, STAR_Y);
  tft.print('*');
}

void clearStars() {
  tft.fillRect(STAR_X0, STAR_Y, CODE_LEN * STAR_W, STAR_H, ST77XX_BLACK);
}

void resetCode() {
  codeIndex = 0;
  memset(code, 0, sizeof(code));
}

// Change state locally and tell the peer
void setState(uint8_t s, bool notify = true) {
  if (s == state) return;
  state = s;
  if (state == MSG_DISARM) disarmStart = millis();
  resetCode();
  clearStars();
  Serial.println(state == MSG_DISARM ? "DISARM" : "ARM");
  if (notify && NowSerial.availableForWrite()) NowSerial.write(state);
}

void handlePress(uint8_t pad) {
  if (pad == CLEAR_PAD) {
    resetCode();
    clearStars();
    Serial.println("Entry cleared");
    return;
  }
  if (pad > 9 || codeIndex >= CODE_LEN) return;

  code[codeIndex] = '0' + pad;
  drawStar(codeIndex);
  codeIndex++;

  if (codeIndex == CODE_LEN) {
    bool ok = (strcmp(code, PASSCODE) == 0);   // compare BEFORE reset
    Serial.println(ok ? "Code OK" : "Wrong code");
    delay(200);
    resetCode();
    clearStars();
    if (ok) setState(MSG_DISARM);
  }
}

void updateLed(uint32_t now) {
  if ((int32_t)(blinkUntil - now) > 0) {
    // disconnect indication: blink blue
    bool on = ((now / BLINK_HALF_MS) % 2) == 0;
    rgbLedWrite(LED, 0, 0, on ? 255 : 0);
  } else if (state == MSG_DISARM) {
    rgbLedWrite(LED, 0, 255, 0);
  } else {
    rgbLedWrite(LED, 255, 0, 0);
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) delay(10);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!cap.begin(0x5A, &Wire)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1) delay(10);
  }
  cap.setAutoconfig(true);

  // screen
  SPI.begin(SCK_PIN, -1, MOSI_PIN, CS_PIN);
  tft.init(240, 240, SPI_MODE3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(40, 100);
  tft.print("READY");       // proves the display works
  delay(1000);
  tft.fillScreen(ST77XX_BLACK);

  WiFi.mode(ESPNOW_WIFI_MODE);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  while (!(WiFi.STA.started() || WiFi.AP.started())) delay(100);
  Serial.println(WiFi.macAddress());

  NowSerial.begin(115200);

  blinkUntil = millis() + BLINK_TOTAL_MS;  // start "disconnected"
}

void loop() {
  uint32_t now = millis();

  // ---- receive ----
  while (NowSerial.available()) {
    int b = NowSerial.read();
    if (b < 0) break;
    lastRx = now;
    if (b == MSG_DISARM || b == MSG_ARM) setState((uint8_t)b, false);
    // 'H' just counts as "alive"
  }

  // ---- connection tracking ----
  bool nowConnected = (lastRx != 0) && (now - lastRx < TIMEOUT_MS);
  if (connected && !nowConnected) {
    blinkUntil = now + BLINK_TOTAL_MS;   // blink, then fall back to state color
    Serial.println("DISCONNECTED");
  } else if (!connected && nowConnected) {
    Serial.println("CONNECTED");
  }
  connected = nowConnected;

  // ---- heartbeat ----
  if (now - lastHeartbeat >= HEARTBEAT_MS && NowSerial.availableForWrite()) {
    NowSerial.write(MSG_HEART);
    lastHeartbeat = now;
  }

  // ---- touch / state logic ----
  currtouched = cap.touched();
  uint16_t pressed = currtouched & ~lasttouched;   // new presses only

  if (state == MSG_ARM) {
    for (uint8_t i = 0; i < 12; i++) {
      if (pressed & _BV(i)) handlePress(i);
    }
  } else if (state == MSG_DISARM) {
    if (pressed || (now - disarmStart >= DISARM_MS)) setState(MSG_ARM);
  }
  lasttouched = currtouched;

  updateLed(now);
  delay(10);
}
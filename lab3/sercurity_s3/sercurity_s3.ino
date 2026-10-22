#include "ESP32_NOW_Serial.h"
#include "MacAddress.h"
#include "WiFi.h"
#include "esp_wifi.h"

// GPIO0 is the BOOT button on most ESP32-S3 DevKits.
// (GPIO48 is often the onboard RGB LED, so it can't also be a button.)
#define BUTTON 20

// Protocol bytes (must match the C6 sketch)
#define ARM        2
#define DISARM     1
#define MSG_HEART  'H'

#define HEARTBEAT_MS   500
#define TIMEOUT_MS     2000
#define BLINK_TOTAL_MS 3000
#define BLINK_HALF_MS  250
#define DEBOUNCE_MS    50

#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_WIFI_MODE WIFI_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA

#define RGB_BUILTIN 38
#define DISARM_MS      30000

// C6 - A0:85:E3:DA:BD:F8
const MacAddress peer_mac({0xA0, 0x85, 0xE3, 0xDA, 0xBD, 0xF8});

ESP_NOW_Serial_Class NowSerial(peer_mac, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);

uint8_t  state = ARM;          // only ever ARM or DISARM
bool     connected = false;
uint32_t lastRx = 0, lastHeartbeat = 0, blinkUntil = 0, lastPress = 0;
bool     lastButton = HIGH;
uint32_t disarmStart = 0;

void sendState() {
  if (NowSerial.availableForWrite()) NowSerial.write(state);
}

void setState(uint8_t s, bool notify) {
  if (s == state) return;
  state = s;
  if (state == DISARM) disarmStart = millis();
  Serial.println(state == DISARM ? "DISARM" : "ARM");
  if (notify) sendState();
}

void updateLed(uint32_t now) {
  if ((int32_t)(blinkUntil - now) > 0) {
    bool on = ((now / BLINK_HALF_MS) % 2) == 0;
    rgbLedWrite(RGB_BUILTIN, 0, 0, on ? 255 : 0);   // disconnect blink
  } else if (state == DISARM) {
    rgbLedWrite(RGB_BUILTIN, 0, 255, 0);
  } else {
    rgbLedWrite(RGB_BUILTIN, 255, 0, 0);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLDOWN);

  WiFi.mode(ESPNOW_WIFI_MODE);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  while (!(WiFi.STA.started() || WiFi.AP.started())) delay(100);

  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  NowSerial.begin(115200);
  lastButton = digitalRead(BUTTON);

  blinkUntil = millis() + BLINK_TOTAL_MS;   // start "disconnected"
}

void loop() {
  uint32_t now = millis();

  // ---- receive ----
  while (NowSerial.available()) {
    int b = NowSerial.read();
    if (b < 0) break;
    lastRx = now;
    if (b == ARM || b == DISARM) setState((uint8_t)b, false);
    // 'H' only means "alive"
  }

  // ---- connection tracking ----
  bool nowConnected = (lastRx != 0) && (now - lastRx < TIMEOUT_MS);
  if (connected && !nowConnected) {
    blinkUntil = now + BLINK_TOTAL_MS;
    Serial.println("DISCONNECTED");
  } else if (!connected && nowConnected) {
    Serial.println("CONNECTED");
    sendState();   // resync after reconnect
  }
  connected = nowConnected;

  // ---- heartbeat ----
  if (now - lastHeartbeat >= HEARTBEAT_MS && NowSerial.availableForWrite()) {
    NowSerial.write(MSG_HEART);
    lastHeartbeat = now;
  }

  // ---- button (edge-detected, debounced) ----
  bool button = digitalRead(BUTTON);
  if (lastButton == HIGH && button == LOW && now - lastPress > DEBOUNCE_MS) {
    lastPress = now;
    setState(state == ARM ? DISARM : ARM, true);
  }
  lastButton = button;

  if (state == DISARM) {
    if ((now - disarmStart >= DISARM_MS)) setState(ARM, true);
  }

  updateLed(now);
  delay(10);
}
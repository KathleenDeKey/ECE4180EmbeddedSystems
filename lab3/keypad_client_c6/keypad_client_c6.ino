// keypad_node.ino — MPR121 capacitive keypad -> BLE -> USB dongle
//
// Keypad init and scan logic is taken from the known-working MPR121 test
// sketch. BLE logic is the same as before: sends key-down / key-up events
// (not just "taps"), so holding a key auto-repeats on the PC and multi-key
// combos work.

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// ---------------- Wiring ----------------
#define SDA_PIN 6
#define SCL_PIN 9

// What each pad sends. Printable ASCII is typed as-is.
const uint8_t KEYMAP[12] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#'
};

// ---------------- Shared protocol (must match dongle) ----------------
static BLEUUID serviceUUID("7a1c0001-5b2e-4c3a-9f10-2d4e6a8b0c11");
static BLEUUID charUUID   ("7a1c0002-5b2e-4c3a-9f10-2d4e6a8b0c11");
#define PKT_KEY_DN  'D'
#define PKT_KEY_UP  'U'
#define PKT_KEY_ALL 'A'
// ---------------------------------------------------------------------

static BLEClient *client = nullptr;
static BLERemoteCharacteristic *rc = nullptr;
static BLEAdvertisedDevice *target = nullptr;
static volatile bool linked = false;
static volatile bool needResync = false;   // set on (re)link, handled in loop()

Adafruit_MPR121 cap = Adafruit_MPR121();

// Same state variables as the working sketch
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

class ClientCB : public BLEClientCallbacks {
  void onConnect(BLEClient *c) {}
  void onDisconnect(BLEClient *c) { linked = false; }
};

class AdvCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) {
    if (d.haveServiceUUID() && d.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      if (target) delete target;
      target = new BLEAdvertisedDevice(d);
    }
  }
};

static void sendPkt(const uint8_t *pkt, size_t len) {
  // Write WITH response: a lost key-up would mean a stuck key.
  Serial.print("sending");
  if (linked && rc) rc->writeValue((uint8_t *)pkt, len, true);
}

static bool linkToDongle() {
  if (target) { delete target; target = nullptr; }
  BLEScan *scan = BLEDevice::getScan();
  scan->start(2, false);
  scan->clearResults();
  if (!target) return false;

  if (!client) {
    client = BLEDevice::createClient();
    client->setClientCallbacks(new ClientCB());
  }
  if (!client->connect(target)) return false;

  BLERemoteService *svc = client->getService(serviceUUID);
  rc = svc ? svc->getCharacteristic(charUUID) : nullptr;
  if (!rc) { client->disconnect(); return false; }

  linked = true;
  Serial.println("Linked to dongle");

  // Fresh start: clear the dongle's keys, and forget our "stable" state so any
  // pad that is physically held right now gets re-sent as a key-down.
  const uint8_t all[1] = {PKT_KEY_ALL};
  sendPkt(all, 1);
  needResync = true;
  return true;
}

// Scanning/connecting blocks for seconds, so it runs in its own task.
// loop() keeps reading the pads (and printing touches) the whole time.
static void bleTask(void *param) {
  for (;;) {
    if (!linked) linkToDongle();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);

  // Wait up to 3 seconds for the serial monitor, but don't hang forever
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }
  delay(500);

  Serial.println("MPR121 BLE keypad node");

  // Tell the ESP32-S3 which pins to use
  Wire.begin(SDA_PIN, SCL_PIN);

  // Default address is 0x5A
  if (!cap.begin(0x5A, &Wire)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1) delay(10);
  }
  Serial.println("MPR121 found!");

  Serial.println("Running auto configuration.");
  cap.setAutoconfig(true);
  Serial.println("Keypad ready, touch a pad...");

  // BLE setup
  BLEDevice::init("Keypad-Node");
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvCB());
  scan->setInterval(100);
  scan->setWindow(99);
  scan->setActiveScan(true);

  xTaskCreate(bleTask, "bleTask", 8192, nullptr, 1, nullptr);
}

void loop() {
  if (needResync) {          // just (re)linked: re-send any pad held right now
    lasttouched = 0;
    needResync = false;
  }

  // Get the currently touched pads
  currtouched = cap.touched();

  for (uint8_t i = 0; i < 12; i++) {
    currtouched = cap.touched();

    for (uint8_t i = 0; i < 12; i++) {
    // if it *is* touched and *wasn't* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {
      Serial.print(i); Serial.println(" touched");
      uint8_t pkt[2] = {PKT_KEY_DN, KEYMAP[i]};
      sendPkt(pkt, 2);
    }
    // if it *was* touched and now *isn't*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i))) {
      Serial.print(i); Serial.println(" released");
    }
  }
  } 

  // reset our state
  lasttouched = currtouched;

  delay(100);
}
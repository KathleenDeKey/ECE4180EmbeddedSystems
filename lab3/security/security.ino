#include <Arduino.h>
#include "BLEDevice.h"

// GPIO0 is the BOOT button on most ESP32-S3 DevKits.
// (GPIO48 is often the onboard RGB LED, so it can't also be a button.)
#define BUTTON 20

// Protocol bytes (must match the C6 keypad sketch)
#define ARM        2
#define DISARM     1
#define MSG_HEART  'H'

#define HEARTBEAT_MS   500
#define TIMEOUT_MS     2000
#define BLINK_TOTAL_MS 3000
#define BLINK_HALF_MS  250
#define DEBOUNCE_MS    50
#define DISARM_MS      30000

#undef RGB_BUILTIN
#define RGB_BUILTIN 38

// ---- BLE (same UUIDs as the keypad server) ----
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static boolean doConnect = false;
static boolean bleConnected = false;   // BLE link up
static boolean doScan = false;
static BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;
static BLEAdvertisedDevice *myDevice = nullptr;

uint8_t  state = ARM;          // only ever ARM or DISARM
bool     connected = false;    // BLE link up AND heartbeat recent
uint32_t lastRx = 0, lastHeartbeat = 0, blinkUntil = 0, lastPress = 0;
bool     lastButton = HIGH;
uint32_t disarmStart = 0;

// Ring buffer: BLE task -> loop()
#define RX_BUF_SIZE 32
volatile uint8_t rxBuf[RX_BUF_SIZE];
volatile uint8_t rxHead = 0, rxTail = 0;

void rxPush(uint8_t b) {
  uint8_t next = (rxHead + 1) % RX_BUF_SIZE;
  if (next == rxTail) return;  // full, drop
  rxBuf[rxHead] = b;
  rxHead = next;
}

bool rxPop(uint8_t &b) {
  if (rxTail == rxHead) return false;
  b = rxBuf[rxTail];
  rxTail = (rxTail + 1) % RX_BUF_SIZE;
  return true;
}

// Notifications from the keypad
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  for (size_t i = 0; i < length; i++) rxPush(pData[i]);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient) {}

  void onDisconnect(BLEClient *pclient) {
    bleConnected = false;
    doScan = true;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(myDevice)) {
    Serial.println("Connect failed");
    return false;
  }

  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service not found");
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Characteristic not found");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  bleConnected = true;
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    }
  }
};

void bleSend(uint8_t b) {
  if (bleConnected && pRemoteCharacteristic) {
    pRemoteCharacteristic->writeValue(&b, 1, false);
  }
}

void sendState() {
  bleSend(state);
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

  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  lastButton = digitalRead(BUTTON);
  blinkUntil = millis() + BLINK_TOTAL_MS;   // start "disconnected"
}

void loop() {
  uint32_t now = millis();

  // ---- BLE connect / rescan (per the example) ----
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("Failed to connect; will rescan.");
      doScan = true;
    }
    doConnect = false;
  }
  if (!bleConnected && doScan) {
    BLEDevice::getScan()->start(0);   // scan until the keypad is found
    doScan = false;
  }

  // ---- receive ----
  uint8_t b;
  while (rxPop(b)) {
    lastRx = now;
    if (b == ARM || b == DISARM) setState(b, false);
    // 'H' only means "alive"
  }

  // ---- connection tracking ----
  if (!bleConnected) lastRx = 0;   // link dropped: forget last heartbeat
  bool nowConnected = bleConnected && (lastRx != 0) && (now - lastRx < TIMEOUT_MS);
  if (connected && !nowConnected) {
    blinkUntil = now + BLINK_TOTAL_MS;
    Serial.println("DISCONNECTED");
  } else if (!connected && nowConnected) {
    Serial.println("CONNECTED");
    sendState();   // resync after reconnect
  }
  connected = nowConnected;

  // ---- heartbeat ----
  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    bleSend(MSG_HEART);
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
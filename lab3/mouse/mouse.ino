// mouse_node.ino — 5-way nav switch -> BLE -> USB dongle
//
// Same Button-struct + shared-ISR pattern as your CH9328 sketch, but instead
// of tapping arrow keys it drives the mouse:
//   - a quick tap on a direction nudges the cursor a couple of pixels
//   - holding a direction moves it continuously and accelerates
//   - center = left click (hold it to drag)
//
// Nav switch common -> GND, each direction -> its GPIO (internal pull-ups).

#include <Arduino.h>
#include <BLEDevice.h>

// ---------------- Wiring ----------------
#define UP      15
#define DOWN    16
#define LEFT    17
#define RIGHT   18
#define CENTER  19

#define DEBOUNCE_MS 50

// ---------------- Feel ----------------
#define TICK_MS     10      // send rate while moving (100 Hz)
#define TAP_PX      2       // nudge for a quick tap
#define SPEED_MIN   1.0f    // px per tick when a hold starts
#define SPEED_MAX   14.0f   // px per tick after ACCEL_MS of holding
#define ACCEL_MS    700

// ---------------- Shared protocol (must match dongle) ----------------
static BLEUUID serviceUUID("7a1c0001-5b2e-4c3a-9f10-2d4e6a8b0c11");
static BLEUUID charUUID   ("7a1c0002-5b2e-4c3a-9f10-2d4e6a8b0c11");
#define PKT_MOUSE  'M'
#define BTN_LEFT   0x01
#define BTN_RIGHT  0x02
#define BTN_MIDDLE 0x04

// ---------------- Buttons (same layout as your CH9328 sketch) ----------------
struct Button {
  uint8_t pin;
  int8_t dx, dy;            // direction this button pushes the cursor
  const char *name;
  volatile bool flag;       // set by ISR on a debounced press
  volatile uint32_t last;
};

Button buttons[] = {
  {UP,      0, -1, "up",     false, 0},
  {DOWN,    0,  1, "down",   false, 0},
  {LEFT,   -1,  0, "left",   false, 0},
  {RIGHT,   1,  0, "right",  false, 0},
  {CENTER,  0,  0, "center", false, 0},
};
const size_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);
Button &centerBtn = buttons[NUM_BUTTONS - 1];

// One ISR for every button: set the flag only, nothing else.
// The flag guarantees a tap shorter than one tick is never missed.
void IRAM_ATTR isr(void *arg) {
  Button *b = (Button *)arg;
  uint32_t now = millis();
  if (now - b->last > DEBOUNCE_MS) {
    b->flag = true;
    b->last = now;
  }
}

static inline bool held(const Button &b) { return digitalRead(b.pin) == LOW; }

// ---------------- BLE link to the dongle ----------------
static BLEClient *client = nullptr;
static BLERemoteCharacteristic *rc = nullptr;
static BLEAdvertisedDevice *target = nullptr;
static volatile bool linked = false;

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

static bool linkToDongle() {
  if (target) { delete target; target = nullptr; }
  BLEScan *scan = BLEDevice::getScan();
  scan->start(2, false);
  scan->clearResults();
  if (!target) return false;

  if (!client) {                       // reuse one client (no leak on reconnect)
    client = BLEDevice::createClient();
    client->setClientCallbacks(new ClientCB());
  }
  if (!client->connect(target)) return false;

  BLERemoteService *svc = client->getService(serviceUUID);
  rc = svc ? svc->getCharacteristic(charUUID) : nullptr;
  if (!rc) { client->disconnect(); return false; }

  linked = true;
  Serial.println("Linked to dongle");
  return true;
}

static void sendMouse(uint8_t btns, int8_t dx, int8_t dy, int8_t wheel) {
  uint8_t pkt[5] = {PKT_MOUSE, btns, (uint8_t)dx, (uint8_t)dy, (uint8_t)wheel};
  rc->writeValue(pkt, sizeof(pkt), false);   // write-without-response: low latency
}

// ---------------- Setup / loop ----------------
void setup() {
  Serial.begin(115200);

  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(buttons[i].pin), isr, &buttons[i], FALLING);
  }

  BLEDevice::init("Mouse-Node");
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvCB());
  scan->setInterval(100);
  scan->setWindow(99);
  scan->setActiveScan(true);
}

void loop() {
  static uint32_t lastTick = 0, holdStart = 0;
  static bool moving = false;
  static float accX = 0, accY = 0;
  static uint8_t lastBtns = 0;

  if (!linked) {
    lastBtns = 0;                       // dongle released everything; resend on link
    for (auto &b : buttons) b.flag = false;
    linkToDongle();
    return;
  }

  uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  lastTick = now;

  // ---- directions ----
  int dirX = 0, dirY = 0;
  for (size_t i = 0; i < NUM_BUTTONS - 1; i++) {
    Button &b = buttons[i];
    bool tapped = b.flag;
    b.flag = false;
    if (tapped) Serial.println(b.name);

    if (held(b)) {                      // continuous movement
      dirX += b.dx;
      dirY += b.dy;
    } else if (tapped) {                // released before this tick: nudge
      accX += b.dx * TAP_PX;
      accY += b.dy * TAP_PX;
    }
  }

  if (dirX || dirY) {
    if (!moving) { moving = true; holdStart = now; }
    float t = min(1.0f, (now - holdStart) / (float)ACCEL_MS);
    float speed = SPEED_MIN + (SPEED_MAX - SPEED_MIN) * t * t;   // ease-in
    accX += dirX * speed;
    accY += dirY * speed;
  } else {
    moving = false;
  }

  int8_t dx = (int8_t)constrain((int)accX, -127, 127);
  int8_t dy = (int8_t)constrain((int)accY, -127, 127);
  accX -= dx;                           // keep the fractional remainder
  accY -= dy;
  if (!moving && !dx && !dy) accX = accY = 0;

  // ---- center = left click ----
  bool centerTapped = centerBtn.flag;
  centerBtn.flag = false;
  uint8_t btns = held(centerBtn) ? BTN_LEFT : 0;

  if (centerTapped && !btns && !(lastBtns & BTN_LEFT)) {
    // Press + release both happened inside one tick: still send a click.
    Serial.println("center");
    sendMouse(BTN_LEFT, dx, dy, 0);
    sendMouse(0, 0, 0, 0);
    lastBtns = 0;
    return;
  }

  if (dx || dy || btns != lastBtns) {
    if (btns != lastBtns && btns) Serial.println("center");
    sendMouse(btns, dx, dy, 0);
    lastBtns = btns;
  }
}
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Adafruit_MPR121.h"
#include "ICM_20948.h"   // SparkFun ICM-20948 library
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// ================= Pins / addresses =================
// Display (hardware SPI: SCK/MOSI are the board's default SPI pins)
#define CS_PIN   38
#define RST_PIN  40
#define DC_PIN   39

// Shared I2C bus: MPR121 + MCP4725
#define SDA_PIN      8
#define SCL_PIN      9
#define MPR121_ADDR  0x5A
#define DAC_ADDR     0x60   // Adafruit breakout is usually 0x62

#define START_PAD    6      // MPR121 electrode: start / pause / resume / retry

// IMU (ICM-20948) tilt controls, on the same I2C bus
#define AD0_VAL            1      // 1 = address 0x69, 0 = address 0x68
#define TILT_PRESS_MG      500    // tilt past this (in mg, ~30 degrees) to register a move
#define TILT_RELEASE_MG    300    // must come back below this before the next move
#define IMU_EVERY_N_TICKS  2      // read the IMU every 2 ticks (20 ms)
#define IMU_INVERT_X       false  // flip if forward/backward feel reversed
#define IMU_INVERT_Y       false  // flip if left/right feel reversed

// Display orientation: 0 = normal, 2 = rotated 180 degrees (set to 0 to flip it back)
#define DISPLAY_ROTATION 2

// MPR121 electrode for each direction: Up, Down, Left, Right
const uint8_t touchPad[4] = {5, 7, 10, 2};
const char *arrowDirStr[] = {"Up", "Down", "Left", "Right"};

Adafruit_ST7789 tft = Adafruit_ST7789(CS_PIN, DC_PIN, RST_PIN);
Adafruit_MPR121 cap = Adafruit_MPR121();
uint16_t currtouched = 0;

ICM_20948_I2C myICM;
bool imuOk = false;
int imuDir = -1;   // tilt direction currently held: 0 Up, 1 Down, 2 Left, 3 Right, -1 none

SemaphoreHandle_t i2cMutex;   // the audio task and the game both use the I2C bus

// ================= Audio (MCP4725, runs on its own core) =================
struct Note {
  uint16_t freq;  // Hz, 0 = rest
  uint16_t ms;
};
struct SoundReq {
  const Note *notes;
  uint8_t len;
};

// One sound per direction arrow
const Note SND_UP[]    = {{523, 70}, {659, 70}, {784, 140}};   // C5 E5 G5 rising arpeggio
const Note SND_DOWN[]  = {{392, 70}, {330, 70}, {262, 140}};   // G4 E4 C4 falling arpeggio
const Note SND_LEFT[]  = {{587, 90}, {440, 150}};              // D5 -> A4
const Note SND_RIGHT[] = {{440, 90}, {587, 150}};              // A4 -> D5

// Feedback sounds
const Note SND_START[]   = {{523, 80}, {659, 80}, {784, 80}, {1047, 200}};
const Note SND_CORRECT[] = {{784, 50}, {1047, 110}};
const Note SND_LATE[]    = {{440, 60}, {0, 40}, {440, 60}};
const Note SND_LOSE[]    = {{392, 160}, {330, 160}, {262, 160}, {196, 420}};

#define LEN(a) (sizeof(a) / sizeof((a)[0]))
#define PLAY(snd) playSound(snd, LEN(snd))

#define VOLUME        0.8f
#define MIN_SAMPLE_US 60
#define WAVE_SIZE     256

float wave[WAVE_SIZE];
bool dacOk = false;
QueueHandle_t soundQ;

// These are only touched by the audio task
const Note *seq = nullptr;
uint8_t seqLen = 0, seqIdx = 0;
bool playing = false;
uint32_t noteStartUs = 0, lastSampleUs = 0;
float phase = 0;

void dacWrite(uint16_t v) {
  v &= 0x0FFF;
  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  Wire.beginTransmission(DAC_ADDR);
  Wire.write((v >> 8) & 0x0F);
  Wire.write(v & 0xFF);
  Wire.endTransmission(true);
  xSemaphoreGive(i2cMutex);
}

// Safe to call from the game loop: just hands the request to the audio task
void playSound(const Note *s, uint8_t len) {
  if (!dacOk) return;
  SoundReq r = {s, len};
  xQueueOverwrite(soundQ, &r);
}

void audioUpdate() {
  if (!playing) return;

  uint32_t now = micros();
  uint32_t dtUs = now - lastSampleUs;
  if (dtUs < MIN_SAMPLE_US) return;
  lastSampleUs = now;

  uint32_t elapsedUs = now - noteStartUs;
  while (elapsedUs >= (uint32_t)seq[seqIdx].ms * 1000UL) {
    noteStartUs += (uint32_t)seq[seqIdx].ms * 1000UL;
    seqIdx++;
    if (seqIdx >= seqLen) {
      playing = false;
      dacWrite(0);
      return;
    }
    elapsedUs = now - noteStartUs;
  }

  const Note &n = seq[seqIdx];
  if (n.freq == 0) {
    dacWrite(0);
    return;
  }

  float t = elapsedUs / 1000.0f;
  float attack = (t < 5.0f) ? (t / 5.0f) : 1.0f;
  float env = attack * (1.0f - t / n.ms);

  phase += n.freq * (dtUs * 1e-6f);
  phase -= (int)phase;
  float s = wave[(int)(phase * WAVE_SIZE) & (WAVE_SIZE - 1)];

  float out = env * VOLUME * (0.5f + 0.5f * s) * 4095.0f;
  dacWrite((uint16_t)out);
}

void audioTask(void *arg) {
  for (;;) {
    SoundReq req;
    if (xQueueReceive(soundQ, &req, 0) == pdTRUE) {
      seq = req.notes;
      seqLen = req.len;
      seqIdx = 0;
      phase = 0;
      noteStartUs = micros();
      lastSampleUs = noteStartUs;
      playing = true;
    }
    if (playing) audioUpdate();
    else vTaskDelay(1);
  }
}

const Note *dirNotes[4] = {SND_UP, SND_DOWN, SND_LEFT, SND_RIGHT};
const uint8_t dirLens[4] = {LEN(SND_UP), LEN(SND_DOWN), LEN(SND_LEFT), LEN(SND_RIGHT)};

// ================= Display =================
#define SCREEN_W  240
#define SCREEN_H  240
#define BAR_H     24      // score bar at the top
#define LANE_W    60
#define ARROW_R   18      // half-size of an arrow
#define START_Y   46      // arrow spawn height (centre)
#define HIT_Y     200     // arrow centre when it should be hit

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define COLOR_GHOST   RGB565(55, 55, 70)
#define COLOR_DIVIDER RGB565(45, 45, 45)
#define COLOR_PERFECT RGB565(255, 220, 0)
#define COLOR_GREAT   RGB565(60, 220, 60)
#define COLOR_GOOD    RGB565(120, 200, 255)
#define COLOR_LATE    RGB565(255, 140, 0)
#define COLOR_EARLY   RGB565(150, 150, 150)

// Lanes left -> right: Left, Down, Up, Right
const uint8_t dirLane[4] = {2, 1, 0, 3};   // indexed by direction (Up, Down, Left, Right)
const uint16_t dirColor[4] = {
  RGB565(60, 220, 60),    // Up    green
  RGB565(60, 140, 255),   // Down  blue
  RGB565(200, 80, 255),   // Left  purple
  RGB565(255, 70, 70)     // Right red
};

int arrowDrawnY = -1000;       // where the moving arrow is currently drawn
uint32_t feedbackUntil = 0;

int laneX(int dir) { return dirLane[dir] * LANE_W + LANE_W / 2; }

void drawArrow(int cx, int cy, int dir, uint16_t color) {
  const int r = ARROW_R;
  switch (dir) {
    case 0:  // up
      tft.fillTriangle(cx, cy - r, cx - r, cy, cx + r, cy, color);
      tft.fillRect(cx - 8, cy, 17, 16, color);
      break;
    case 1:  // down
      tft.fillTriangle(cx, cy + r, cx - r, cy, cx + r, cy, color);
      tft.fillRect(cx - 8, cy - 16, 17, 16, color);
      break;
    case 2:  // left
      tft.fillTriangle(cx - r, cy, cx, cy - r, cx, cy + r, color);
      tft.fillRect(cx, cy - 8, 16, 17, color);
      break;
    case 3:  // right
      tft.fillTriangle(cx + r, cy, cx, cy - r, cx, cy + r, color);
      tft.fillRect(cx - 16, cy - 8, 16, 17, color);
      break;
  }
}

void eraseArrow(int cx, int cy) {
  tft.fillRect(cx - ARROW_R - 1, cy - ARROW_R - 1, 2 * ARROW_R + 3, 2 * ARROW_R + 3, ST77XX_BLACK);
}

void drawField() {
  tft.fillRect(0, BAR_H, SCREEN_W, SCREEN_H - BAR_H, ST77XX_BLACK);
  for (int i = 1; i < 4; i++) {
    tft.drawFastVLine(i * LANE_W, BAR_H, SCREEN_H - BAR_H, COLOR_DIVIDER);
  }
  for (int d = 0; d < 4; d++) {
    drawArrow(laneX(d), HIT_Y, d, COLOR_GHOST);   // target outlines on the hit line
  }
  arrowDrawnY = -1000;
}

int score = 0;

void drawScore() {
  char buf[20];
  snprintf(buf, sizeof(buf), "SCORE %-5d", score);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 5);
  tft.print(buf);
}

// pts >= 0: big "+points" with a small rating word beside it
// pts <  0: just the text (used for EARLY / LATE warnings)
void showFeedback(const char *text, int pts, uint16_t color) {
  tft.fillRect(140, 0, SCREEN_W - 140, BAR_H, ST77XX_BLACK);
  tft.setTextColor(color);
  if (pts >= 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "+%d", pts);
    tft.setTextSize(2);
    tft.setCursor(140, 5);
    tft.print(buf);
    tft.setTextSize(1);
    tft.setCursor(192, 9);
    tft.print(text);
  } else {
    tft.setTextSize(2);
    tft.setCursor(140, 5);
    tft.print(text);
  }
  feedbackUntil = millis() + 700;
}

void clearFeedback() {
  tft.fillRect(140, 0, SCREEN_W - 140, BAR_H, ST77XX_BLACK);
  feedbackUntil = 0;
}

void showMessage(const char *l1, const char *l2) {
  tft.fillRect(0, 90, SCREEN_W, 70, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor((SCREEN_W - strlen(l1) * 12) / 2, 100);
  tft.print(l1);
  if (l2) {
    tft.setCursor((SCREEN_W - strlen(l2) * 12) / 2, 130);
    tft.print(l2);
  }
}

// ================= Game =================
#define TICK_MS         10
#define GAP_TICKS       40     // gap between arrows
#define HIT_TICKS       150    // arrow reaches the hit line after 1.5 s
#define HIT_WINDOW      30     // +/- 0.3 s counts as a hit
#define PERFECT_WINDOW  5      // +/- 0.05 s is PERFECT
#define GREAT_WINDOW    15     // +/- 0.15 s is GREAT

typedef enum {
  STATE_START,
  STATE_ON_TIME,
  STATE_LATE,
  STATE_CORRECT,
  STATE_LOSE,
  STATE_PAUSE
} state;

state currState = STATE_START;
state pausedFrom = STATE_START;

long numTicks = 0;
int ticks_late = HIT_TICKS + HIT_WINDOW;   // arrow has passed the hit window
int ticks_lose = 200;                      // arrow has left the screen
int randDirection = 0;
uint32_t nextTickMs = 0;

void renderArrow() {
  int y = START_Y + (int)(((long)(HIT_Y - START_Y) * numTicks) / HIT_TICKS);
  if (y == arrowDrawnY) return;
  int x = laneX(randDirection);

  if (arrowDrawnY > -500) eraseArrow(x, arrowDrawnY);
  // repaint the target outline when the arrow is near it
  if (abs(y - HIT_Y) < 40 || abs(arrowDrawnY - HIT_Y) < 40) {
    drawArrow(x, HIT_Y, randDirection, COLOR_GHOST);
  }
  drawArrow(x, y, randDirection, dirColor[randDirection]);
  arrowDrawnY = y;
}

void clearArrow() {
  if (arrowDrawnY > -500) {
    int x = laneX(randDirection);
    eraseArrow(x, arrowDrawnY);
    drawArrow(x, HIT_Y, randDirection, COLOR_GHOST);
  }
  arrowDrawnY = -1000;
}

void enterStart() {
  currState = STATE_START;
  drawField();
  showMessage("RHYTHM GAME", "TOUCH 6 TO START");
  Serial.println("START - touch pad 6");
}

void newArrow() {
  randDirection = random(0, 4);
  numTicks = 0;
  arrowDrawnY = -1000;
  Serial.printf("ARROW: %s\n", arrowDirStr[randDirection]);
  playSound(dirNotes[randDirection], dirLens[randDirection]);
}

// Returns a direction (0 Up, 1 Down, 2 Left, 3 Right) only at the moment a tilt
// starts, or -1. Same idea as "just touched": hold or return to neutral between moves.
//   tilt right    (-Y) -> Up        tilt left     (+Y) -> Down
//   tilt forward  (+X) -> Left      tilt backward (-X) -> Right
int readImuPress() {
  static uint8_t divider = 0;
  if (!imuOk) return -1;
  if (++divider < IMU_EVERY_N_TICKS) return -1;
  divider = 0;

  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  myICM.getAGMT();
  xSemaphoreGive(i2cMutex);
  if (myICM.status != ICM_20948_Stat_Ok) return -1;

  float ax = myICM.accX() * (IMU_INVERT_X ? -1.0f : 1.0f);
  float ay = myICM.accY() * (IMU_INVERT_Y ? -1.0f : 1.0f);

  // Hysteresis: easier to stay in a tilt than to enter one
  float thr = (imuDir >= 0) ? TILT_RELEASE_MG : TILT_PRESS_MG;
  int dir = -1;
  if (fabsf(ax) >= thr || fabsf(ay) >= thr) {
    if (fabsf(ax) >= fabsf(ay)) dir = (ax > 0) ? 2 : 3;   // forward = Left, backward = Right
    else                        dir = (ay > 0) ? 1 : 0;   // left = Down, right = Up
  }

  int press = (dir >= 0 && dir != imuDir) ? dir : -1;
  imuDir = dir;
  if (press >= 0) Serial.printf("IMU: %s\n", arrowDirStr[press]);
  return press;
}

void gameTick() {
  uint16_t prevTouched = currtouched;
  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  currtouched = cap.touched();
  xSemaphoreGive(i2cMutex);
  uint16_t justTouched = currtouched & ~prevTouched;   // new touches only
  int imuPress = readImuPress();                       // new tilt, or -1
  bool startTouch = justTouched & _BV(START_PAD);

  if (feedbackUntil && millis() > feedbackUntil) clearFeedback();

  // Pause
  if (startTouch && (currState == STATE_ON_TIME || currState == STATE_LATE || currState == STATE_CORRECT)) {
    pausedFrom = currState;
    currState = STATE_PAUSE;
    showMessage("PAUSED", "TOUCH 6 TO RESUME");
    Serial.println("PAUSE");
    return;
  }

  switch (currState) {
    case STATE_START:
      if (startTouch) {
        randomSeed(micros());
        score = 0;
        drawScore();
        clearFeedback();
        drawField();
        PLAY(SND_START);
        numTicks = -20;              // let the jingle finish before the first arrow
        currState = STATE_CORRECT;   // CORRECT doubles as the "wait" state
        Serial.println("Start");
      }
      break;

    case STATE_ON_TIME:
    case STATE_LATE:
      numTicks++;
      // Correct direction from either the touch pad or an IMU tilt
      if ((justTouched & _BV(touchPad[randDirection])) || imuPress == randDirection) {
        if (numTicks < HIT_TICKS - HIT_WINDOW) {
          showFeedback("EARLY", -1, COLOR_EARLY);   // too soon: arrow keeps falling
          break;
        }
        // The closer to the exact moment, the more points:
        // 100 dead-on, falling linearly to 10 at the edge of the window, 5 if late
        int d = abs((int)numTicks - HIT_TICKS);
        const char *rating;
        uint16_t col;
        int pts;
        if (d <= HIT_WINDOW) {
          pts = 10 + (90 * (HIT_WINDOW - d)) / HIT_WINDOW;
          if (d <= PERFECT_WINDOW)    { rating = "PERFECT"; col = COLOR_PERFECT; }
          else if (d <= GREAT_WINDOW) { rating = "GREAT";   col = COLOR_GREAT; }
          else                        { rating = "GOOD";    col = COLOR_GOOD; }
        } else {
          pts = 5; rating = "LATE"; col = COLOR_LATE;
        }
        score += pts;
        drawScore();
        showFeedback(rating, pts, col);
        clearArrow();
        PLAY(SND_CORRECT);
        Serial.printf("%s  +%d  (off by %d ticks)  score: %d\n", rating, pts, d, score);
        numTicks = 0;
        currState = STATE_CORRECT;
        break;
      }
      if (currState == STATE_ON_TIME && numTicks > ticks_late) {
        Serial.println("You're late");
        PLAY(SND_LATE);
        currState = STATE_LATE;
      } else if (currState == STATE_LATE && numTicks > ticks_lose) {
        Serial.printf("You lose!  final score: %d\n", score);
        PLAY(SND_LOSE);
        drawField();
        showMessage("GAME OVER", "TOUCH 6 TO RETRY");
        currState = STATE_LOSE;
      }
      break;

    case STATE_CORRECT:
      numTicks++;
      if (numTicks > GAP_TICKS) {
        newArrow();
        currState = STATE_ON_TIME;
      }
      break;

    case STATE_LOSE:
      if (startTouch) enterStart();
      break;

    case STATE_PAUSE:
      if (startTouch) {
        Serial.println("RESUME");
        drawField();             // wipes the pause message; arrow redraws next tick
        currState = pausedFrom;
      }
      break;
  }

  if (currState == STATE_ON_TIME || currState == STATE_LATE) renderArrow();
}

// ================= Arduino =================
void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) delay(10);
  delay(500);

  i2cMutex = xSemaphoreCreateMutex();
  soundQ = xQueueCreate(1, sizeof(SoundReq));

  // Display
  tft.init(240, 240);
  tft.setRotation(DISPLAY_ROTATION);
  tft.setSPISpeed(40000000);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  drawScore();

  // Shared I2C bus
  Wire.setPins(SDA_PIN, SCL_PIN);
  Wire.begin();
  Wire.setClock(400000);

  // Wavetable: sine plus 25% second harmonic, normalised
  for (int i = 0; i < WAVE_SIZE; i++) {
    float x = 2.0f * PI * i / WAVE_SIZE;
    wave[i] = (sinf(x) + 0.25f * sinf(2.0f * x)) / 1.25f;
  }

  // DAC check
  Wire.beginTransmission(DAC_ADDR);
  if (Wire.endTransmission() == 0) {
    dacOk = true;
    dacWrite(0);
    Serial.println("DAC found");
  } else {
    Serial.println("DAC not found at that address - game will run silently");
  }

  // IMU (try a few times, then carry on without it)
  for (int i = 0; i < 5 && !imuOk; i++) {
    myICM.begin(Wire, AD0_VAL);
    if (myICM.status == ICM_20948_Stat_Ok) imuOk = true;
    else delay(200);
  }
  Serial.println(imuOk ? "IMU found" : "IMU not found - tilt controls disabled");

  // Touch sensor
  if (!cap.begin(MPR121_ADDR, &Wire)) {
    Serial.println("MPR121 not found, check wiring?");
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 100);
    tft.print("MPR121 NOT FOUND");
    while (1) delay(10);
  }
  cap.setAutoconfig(true);
  Serial.println("MPR121 found");

  // Audio runs on core 0 so display drawing on core 1 can't make it crackle
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, NULL, 2, NULL, 0);

  PLAY(SND_START);   // startup chime doubles as a speaker test
  enterStart();
  nextTickMs = millis();
}

void loop() {
  if ((int32_t)(millis() - nextTickMs) >= 0) {
    nextTickMs += TICK_MS;
    gameTick();
  }
}
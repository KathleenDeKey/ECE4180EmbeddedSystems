#define RED_OUT     0x8   // GPIO35
#define GREEN_OUT   0x10  // GPIO36
#define BLUE_OUT    0x20  // GPIO37
#define YELLOW_OUT  0x18
#define ALL_MASK    0x38

int state;              // 0 = red, 1 = green, 2 = blue, 3 = yellow
int prevPos, prevNeg;   // previous button levels, for edge detection

void setup() {
  Serial.begin(115200);

  // Inline assembly for Xtensa (ESP32-S3): configure GPIO35/36/37 as
  // function-out pins, GPIO47/48 as input+pulldown, enable outputs 35-37,
  // and clear them initially.
  asm volatile (
    // GPIO35_CONFIG |= 0x1000
    "movi a2, 0x60009090 \n\t"
    "l32i a3, a2, 0      \n\t"
    "movi a4, 0x1000     \n\t"
    "or   a3, a3, a4     \n\t"
    "s32i a3, a2, 0      \n\t"

    // GPIO36_CONFIG |= 0x1000
    "movi a2, 0x60009094 \n\t"
    "l32i a3, a2, 0      \n\t"
    "or   a3, a3, a4     \n\t"
    "s32i a3, a2, 0      \n\t"

    // GPIO37_CONFIG |= 0x1000
    "movi a2, 0x60009098 \n\t"
    "l32i a3, a2, 0      \n\t"
    "or   a3, a3, a4     \n\t"
    "s32i a3, a2, 0      \n\t"

    // GPIO48_CONFIG |= 0x1280
    "movi a2, 0x600090C4 \n\t"
    "l32i a3, a2, 0      \n\t"
    "movi a4, 0x1280     \n\t"
    "or   a3, a3, a4     \n\t"
    "s32i a3, a2, 0      \n\t"

    // GPIO47_CONFIG |= 0x1280
    "movi a2, 0x600090C0 \n\t"
    "l32i a3, a2, 0      \n\t"
    "or   a3, a3, a4     \n\t"
    "s32i a3, a2, 0      \n\t"

    // GPIO_OUTPUT_ENABLE |= 0x38
    "movi a2, 0x6000402C \n\t"
    "l32i a3, a2, 0      \n\t"
    "movi a4, 0x38       \n\t"
    "or   a3, a3, a4     \n\t"
    "s32i a3, a2, 0      \n\t"

    // GPIO_OUTPUT_CLEAR = 0x38 (write-1-to-clear register; plain store)
    "movi a2, 0x60004018 \n\t"
    "movi a3, 0x38       \n\t"
    "s32i a3, a2, 0      \n\t"
    :
    :
    : "a2", "a3", "a4", "memory"
  );

  state = 0;
  prevPos = 0;
  prevNeg = 0;
}

// Reads GPIO_INPUT via inline assembly, returns the raw 32-bit value
int readGpioInput() {
  int val;
  asm volatile (
    "movi a2, 0x60004040 \n\t"   // GPIO_INPUT address
    "l32i %0, a2, 0      \n\t"
    : "=r" (val)
    :
    : "a2"
  );
  return val;
}

// Drives the RGB output pins: clears bits 3-5, then sets the given mask
void driveColor(int setMask) {
  asm volatile (
    "movi a2, 0x60004018 \n\t"   // GPIO_OUTPUT_CLEAR
    "movi a3, 0x38       \n\t"
    "s32i a3, a2, 0      \n\t"
    :
    :
    : "a2", "a3", "memory"
  );
  asm volatile (
    "movi a2, 0x60004014 \n\t"   // GPIO_OUTPUT_SET
    "s32i %0, a2, 0      \n\t"
    :
    : "r" (setMask)
    : "a2", "memory"
  );
}

void loop() {
  int gpioInput = readGpioInput();

  int posNow = (gpioInput & (1 << (47 - 32))) != 0;
  int negNow = (gpioInput & (1 << (48 - 32))) != 0;

  // rising-edge detect: only act on the transition into "pressed"
  if (posNow && !prevPos) {
    state = state + 1;
    Serial.print("positive button pressed\n");
  } else if (negNow && !prevNeg) {
    state = state - 1;
    Serial.print("negative button pressed\n");
  }
  prevPos = posNow;
  prevNeg = negNow;

  if (state > 3) {
    state = state - 4;
  } else if (state < 0) {
    state = state + 4;
  } else if (state == 0) {
    Serial.print("RED\n");
    driveColor(RED_OUT);
  } else if (state == 1) {
    Serial.print("GREEN\n");
    driveColor(GREEN_OUT);
  } else if (state == 2) {
    Serial.print("BLUE\n");
    driveColor(BLUE_OUT);
  } else if (state == 3) {
    Serial.print("YELLOW\n");
    driveColor(YELLOW_OUT);
  }
}
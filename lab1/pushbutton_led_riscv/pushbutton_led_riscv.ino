void setup() {
  Serial.begin(115200);

  // Inline assembly for RISC-V-based boards (like ESP32-C3)
  // Configures GPIO7 as output (LED) and GPIO4 as input with pulldown (BUTTON)
  asm volatile (
    "li t0, 0x60091024 \n\t"   // GPIO_OUTPUT_ENABLE_REG
    "lw t1, 0(t0)      \n\t"
    "ori t1, t1, 0x80  \n\t"   // enable output on GPIO7
    "sw t1, 0(t0)      \n\t"

    "li t0, 0x60090020 \n\t"   // IO_MUX_GPIO7_REG
    "lw t1, 0(t0)      \n\t"
    "li t2, 0x1000     \n\t"
    "or t1, t1, t2     \n\t"
    "sw t1, 0(t0)      \n\t"

    "li t0, 0x60090014 \n\t"   // IO_MUX_GPIO4_REG
    "lw t1, 0(t0)      \n\t"
    "li t2, 0x1280     \n\t"   // GPIO function + pulldown enable
    "or t1, t1, t2     \n\t"
    "sw t1, 0(t0)      \n\t"
    :
    :
    : "t0", "t1", "t2", "memory"
  );
}

void loop() {
  int buttonPressed;

  // Inline assembly: read BUTTON (GPIO4) state into buttonPressed
  asm volatile (
    "li t0, 0x6009103C \n\t"   // GPIO_IN_REG
    "lw t1, 0(t0)      \n\t"
    "andi t1, t1, 0x10 \n\t"   // isolate GPIO4 bit
    "srli %0, t1, 4    \n\t"   // normalize to 0/1
    : "=r" (buttonPressed)     // Output operand
    :
    : "t0", "t1"
  );

  if (buttonPressed) {
    asm volatile (
      "li t0, 0x60091008 \n\t" // GPIO_OUT_W1TS_REG (set)
      "li t1, 0x80       \n\t"
      "sw t1, 0(t0)      \n\t"
      :
      :
      : "t0", "t1", "memory"
    );
    Serial.println("button pressed");
  } else {
    asm volatile (
      "li t0, 0x6009100C \n\t" // GPIO_OUT_W1TC_REG (clear)
      "li t1, 0x80       \n\t"
      "sw t1, 0(t0)      \n\t"
      :
      :
      : "t0", "t1", "memory"
    );
    Serial.println("button not pressed");
  }

  delay(100);
}
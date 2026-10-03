# RISC-V assembly (ESP32-C3 style) - direct GPIO register access
# Equivalent to the C setup()/loop() using raw memory-mapped GPIO regs.
#
# Register map used:
#   GPIO_OUTPUT_ENABLE_REG  0x60091024   (bit 7 = GPIO7 output enable)
#   GPIO_OUT_W1TC_REG       0x6009100C   (write 1 to clear GPIO7 -> LED off)
#   GPIO_OUT_W1TS_REG       0x60091008   (write 1 to set GPIO7   -> LED on)
#   IO_MUX_GPIO7_REG        0x60090020   (mux config for GPIO7 as GPIO fn)
#   IO_MUX_GPIO4_REG        0x60090014   (mux config for GPIO4, input+pulldown)
#   GPIO_IN_REG             0x6009103C   (raw input level register)

    .section .text
    .global setup
    .global loop

# ---------------------------------------------------------
# void setup(void)
#   - GPIO7 (LED)    -> output
#   - GPIO4 (BUTTON) -> input, pulldown enabled
# ---------------------------------------------------------
setup:
    # GPIO_OUTPUT_ENABLE |= (1 << 7)
    li      t0, 0x60091024
    lw      t1, 0(t0)
    ori     t1, t1, 0x80
    sw      t1, 0(t0)

    # IO_MUX_GPIO7 |= 0x1000   (select GPIO function for pin 7)
    li      t0, 0x60090020
    lw      t1, 0(t0)
    li      t2, 0x1000
    or      t1, t1, t2
    sw      t1, 0(t0)

    # IO_MUX_GPIO4 |= 0x1280   (GPIO function + pulldown enable for pin 4)
    li      t0, 0x60090014
    lw      t1, 0(t0)
    li      t2, 0x1280
    or      t1, t1, t2
    sw      t1, 0(t0)

    ret

# ---------------------------------------------------------
# void loop(void)
#   - reads BUTTON (GPIO4), drives LED (GPIO7) to match state,
#     then busy-waits ~ delay before looping.
#   NOTE: Serial.print() calls omitted — bare-metal RISC-V has no
#   UART driver here. Replace loop_delay's placeholder with a real
#   UART TX routine if you need the "button pressed" text output.
# ---------------------------------------------------------
loop:
    li      t0, 0x6009103C      # GPIO_IN_REG address
    lw      t1, 0(t0)           # read current input levels
    andi    t1, t1, 0x10        # isolate bit 4 (GPIO4 / BUTTON)
    srli    t1, t1, 4           # normalize to 0 or 1

    bnez    t1, button_pressed

button_not_pressed:
    li      t0, 0x6009100C      # GPIO_OUT_W1TC_REG (clear -> LED off)
    li      t1, 0x80
    sw      t1, 0(t0)
    j       loop_delay

button_pressed:
    li      t0, 0x60091008      # GPIO_OUT_W1TS_REG (set -> LED on)
    li      t1, 0x80
    sw      t1, 0(t0)

loop_delay:
    li      t2, 2000000         # rough busy-wait; tune per clock speed
delay_spin:
    addi    t2, t2, -1
    bnez    t2, delay_spin

    j       loop

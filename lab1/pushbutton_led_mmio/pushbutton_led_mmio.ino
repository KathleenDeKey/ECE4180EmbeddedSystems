int* GPIO_OUTPUT_ENABLE = (int*) 0x60091024;
int* GPIO_OUTPUT_CLEAR = (int*) 0x6009100C;
int* GPIO_OUTPUT_SET = (int*) 0x60091008;
int* IO_MUX_GPIO7 = (int*) 0x60090020;
int* IO_MUX_GPIO4 = (int*) 0x60090014;
int* GPIO_INPUT = (int*) 0x6009103C;

#define LED 5
#define BUTTON 6
void setup() {
  Serial.begin(115200);
  *GPIO_OUTPUT_ENABLE |= 0x80; //set GPIO7 as output
  *IO_MUX_GPIO7 |= 0x1000; // set GPIO7
  *IO_MUX_GPIO4 |= 0x1280; // set GPIO4 as input with pulldown
}

void loop() {
  if(((*GPIO_INPUT & 0x10) >> 4) == 0)  {
    *GPIO_OUTPUT_CLEAR = 0x80;
    Serial.print("button not pressed\n");
    delay(100);
  } else {
    *GPIO_OUTPUT_SET = 0x80;
    Serial.print("button pressed\n");
    delay(100);
  }
  // digitalWrite(LED, HIGH);
  
}
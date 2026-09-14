int* GPIO_OUTPUT_ENABLE = (int*) 0x6000402C;
int* GPIO_OUTPUT_SET = (int*) 0x60004014;
int* GPIO_OUTPUT_CLEAR = (int*) 0x60004018;

int* GPIO_CONFIG = (int*) 0x60009000;
int* GPIO35_CONFIG = (int*) 0x60009090;
int* GPIO36_CONFIG = (int*) 0x60009094;
int* GPIO37_CONFIG = (int*) 0x60009098;

int* GPIO48_CONFIG = (int*) 0x600090C4;
int* GPIO47_CONFIG = (int*) 0x600090C0;

int* GPIO_INPUT = (int*) 0x60004040;

#define RED_OUT 0x8      //GPIO35
#define GREEN_OUT 0x10     //GPIO36
#define BLUE_OUT 0x20    //GPIO37
#define YELLOW_OUT 0x18 

int state;  // 0 = red, 1 = green, 2 = blue, 3 = yellow 

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  *GPIO35_CONFIG |= 0x1000;
  *GPIO36_CONFIG |= 0x1000;
  *GPIO37_CONFIG |= 0x1000;

  *GPIO48_CONFIG |= 0x1280;
  *GPIO47_CONFIG |= 0x1280;

  *GPIO_OUTPUT_ENABLE |= 0x38;
  *GPIO_OUTPUT_CLEAR |= 0x38;

  // pinMode(BUTTON1, INPUT_PULLDOWN);
  // pinMode(BUTTON2, INPUT_PULLDOWN);

  // pinMode(RED_OUT, OUTPUT);
  // pinMode(GREEN_OUT, OUTPUT);
  // pinMode(BLUE_OUT, OUTPUT);

  state = 0;
}

void loop() {
  if((*GPIO_INPUT & (1 << (47 - 32))) != 0) {
    state = state+1;
    Serial.print("positive button pressed\n");
  }
  else if((*GPIO_INPUT & (1 << (48 - 32))) != 0) {
    state = state-1;
    Serial.print("negative button pressed\n");
  }

  // display correct state
  if(state > 3) {
    state = state - 4;
  }
  else if (state < 0) {
    state = state + 4;
  }
  else if (state == 0) {
    Serial.print("RED\n");
    *GPIO_OUTPUT_CLEAR |= 0x38;
    *GPIO_OUTPUT_SET |= RED_OUT;
  }
  else if (state == 1) {
    Serial.print("GREEN\n");
    *GPIO_OUTPUT_CLEAR |= 0x38;
    *GPIO_OUTPUT_SET |= GREEN_OUT;
  }
  else if (state == 2) {
    Serial.print("BLUE\n");
    *GPIO_OUTPUT_CLEAR |= 0x38;
    *GPIO_OUTPUT_SET |= BLUE_OUT;
  }
  else if(state == 3) {
    Serial.print("YELLOW\n");
    *GPIO_OUTPUT_CLEAR |= 0x38;
    *GPIO_OUTPUT_SET |= YELLOW_OUT;
  }

  delay(100);
}

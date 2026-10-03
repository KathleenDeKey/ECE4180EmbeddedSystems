#define UP 12 
#define DOWN 11
#define LEFT 13
#define RIGHT 14
#define CENTER 10

#define DIP1 48
#define DIP2 47
#define DIP3 42
#define DIP4 41
#define DIP5 21
#define DIP6 20
#define DIP7 19
#define DIP8 8

#define POT 1
#define SPEAKER 7

#define RED_OUT 36
#define GREEN_OUT 38
#define BLUE_OUT 39

#define LED 35

long randNumber;
int frequency;
int dip[8];
long numTicks;
int currIndex;

int ticks_late;
int ticks_lose;
int delayTime;

typedef enum {
    STATE_START,
    STATE_CORRECT,
    STATE_ON_TIME,
    STATE_LATE,
    STATE_LOSE,
    STATE_PAUSE
} state;

static state currState = STATE_START;
static state prevState = STATE_START;
const char *arrowDirStr[] = {"Up", "Down", "Left", "Right"};
int randDirection;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(CENTER, INPUT_PULLUP);

  pinMode(DIP1, INPUT_PULLDOWN);
  pinMode(DIP2, INPUT_PULLDOWN);
  pinMode(DIP3, INPUT_PULLDOWN);
  pinMode(DIP4, INPUT_PULLDOWN);
  pinMode(DIP5, INPUT_PULLDOWN);
  pinMode(DIP6, INPUT_PULLDOWN);
  pinMode(DIP7, INPUT_PULLDOWN);
  pinMode(DIP8, INPUT_PULLDOWN);

  pinMode(RED_OUT, OUTPUT);
  pinMode(GREEN_OUT, OUTPUT);
  pinMode(BLUE_OUT, OUTPUT);
  pinMode(LED, OUTPUT);
 
  numTicks = 0;
  currIndex = 0;
  currState = STATE_START;

  ticks_late = 150;
  ticks_lose = 200;

  delayTime = 10;
}

void generateArrow(int arrowDirection) {
  // int userDirection[] = {digitalRead[UP], digitalRead[DOWN], digitalRead[LEFT], digitalRead[RIGHT]};
  int userDirection[4];
  userDirection[0] = !digitalRead(UP);
  userDirection[1] = !digitalRead(DOWN);
  userDirection[2] = !digitalRead(LEFT);
  userDirection[3] = !digitalRead(RIGHT);

  // 0 = Up, 1 = Down, 2 = Left, 3 = Right
  if (numTicks == 0) tone(SPEAKER, frequency, 500);  // play 440 Hz for 500 ms, non-blocking
  
  Serial.printf("%s\n", arrowDirStr[arrowDirection]);
  if (userDirection[arrowDirection]) {
    currState = STATE_CORRECT;
  }
}

void loop() {
  dip[0] = !digitalRead(DIP1);
  dip[1] = !digitalRead(DIP2);
  dip[2] = !digitalRead(DIP3);
  dip[3] = !digitalRead(DIP4);
  dip[4] = !digitalRead(DIP5);
  dip[5] = !digitalRead(DIP6);
  dip[6] = !digitalRead(DIP7);
  dip[7] = !digitalRead(DIP8);
  Serial.println(dip[7]);

    // Need FSM states here
  switch (currState) {
    case STATE_START: 
      Serial.println("START");
      Serial.println("Press Center to Start");
      Serial.println("Adjust tone frequency with potentiometer");
      frequency = analogReadMilliVolts(POT);
      if (!digitalRead(CENTER)) {
        numTicks = 0;
        currIndex = 0;
        prevState = STATE_START;
        currState = STATE_ON_TIME;
        Serial.println("Start");
        delay(100);
      }
      break;
    case STATE_CORRECT:
    Serial.println("CORRECT");
      numTicks = 0;
      currIndex = (currIndex + 1) % 8;
      prevState = STATE_CORRECT;
      currState = STATE_ON_TIME;
      if (!digitalRead(CENTER)) {
        currState = STATE_PAUSE;
      }
      break;
    case STATE_ON_TIME: 
      Serial.printf("ON TIME - index: %d, tick: %d\n", currIndex, numTicks);
 

      if (!digitalRead(CENTER)) {
        prevState = STATE_ON_TIME;
        currState = STATE_PAUSE;
        delay(100);
      } else if (dip[currIndex]) {
        // display green LED
        digitalWrite(RED_OUT, LOW);
        digitalWrite(GREEN_OUT, HIGH);
        digitalWrite(BLUE_OUT, LOW);
        Serial.println("ARROW");
        if (numTicks == 0) {
          randDirection = random(0, 4);  // [0, 3)
        }

        // if it's arrow (dip = 1), then show arrow
        generateArrow(randDirection);
        if (numTicks > ticks_late) {
          Serial.println("You're Late");
          prevState = STATE_ON_TIME;
          currState = STATE_LATE;
        }
      } else {
        // blink LED
        if (numTicks < ticks_late) {
          // write LED high
          Serial.println("Blink");
          digitalWrite(LED, HIGH);
        } else {
          digitalWrite(LED, LOW);
          currState = STATE_CORRECT;
          prevState = STATE_ON_TIME;
        }
      }
      numTicks++;
      break;
    case STATE_LATE: 
    Serial.printf("LATE - index: %d, tick: %d\n", currIndex, numTicks);   
      numTicks++;
      if (!digitalRead(CENTER)) {
        prevState = STATE_LATE;
        currState = STATE_PAUSE;
      } else if (dip[currIndex]) {
        // display yellow LED
        digitalWrite(RED_OUT, HIGH);
        digitalWrite(GREEN_OUT, HIGH);
        digitalWrite(BLUE_OUT, LOW);
        Serial.println("ARROW");
        // if it's arrow (dip = 1), then show arrow
        generateArrow(randDirection);
        if (numTicks > ticks_lose) {
          Serial.println("You Lose!!");

          prevState = STATE_LATE;
          currState = STATE_LOSE;
        }
      } else {
        // blink LED
        if (numTicks < ticks_lose) {
          Serial.println("Blink");
          // write LED high
          digitalWrite(LED, HIGH);
        } else {
          digitalWrite(LED, LOW);
          prevState = STATE_LATE;
          currState = STATE_ON_TIME;
        }
      }
      break;
    case STATE_LOSE:
      Serial.println("LOSE");
      digitalWrite(RED_OUT, HIGH);
      digitalWrite(GREEN_OUT, LOW);
      digitalWrite(BLUE_OUT, LOW);
      Serial.println("Press CENTER to play again.");
      if (!digitalRead(CENTER)) {
        prevState = STATE_LOSE;
        currState = STATE_START;
        delay(50);
      }
      break;
    case STATE_PAUSE:
      Serial.println("PAUSE");
      frequency = analogReadMilliVolts(POT);
      if (!digitalRead(CENTER)) {
        currState = prevState;
        prevState = STATE_PAUSE;
        delay(50);
      }
      break;
  } 
  delay(delayTime);
}
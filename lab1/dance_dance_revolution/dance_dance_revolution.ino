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

#define RED 36
#define GREEN 39
#define BLUE 40

long randNumber;
int tempo;
int dip[8];
long numTicks;
int currStage;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(CENTER, INPUT_PULLUP);

  pinMode(DIP1, INPUT_PULLUP);
  pinMode(DIP2, INPUT_PULLUP);
  pinMode(DIP3, INPUT_PULLUP);
  pinMode(DIP4, INPUT_PULLUP);
  pinMode(DIP5, INPUT_PULLUP);
  pinMode(DIP6, INPUT_PULLUP);
  pinMode(DIP7, INPUT_PULLUP);

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
 
  numTicks = 0;
  currStage = 0;
}

bool read_input() {
  randNumber = random(4);

  if (randNumber == 0) {
    Serial.println("UP");
    if (!digitalRead(UP)) {
      Serial.print("correct");
      currStage++;
    } else {
      Serial.print("wrong");
      while (digitalRead(CENTER)) {
        delay(1);
      }
    }
  }
  else if (randNumber == 1) {
    Serial.println("DOWN");
    if (!digitalRead(DOWN)) {
      Serial.print("correct");
      currStage++;
    } else {
      Serial.print("wrong");
      while (digitalRead(CENTER)) {
        delay(1);
      }
    }
  }
  else if (randNumber == 2) {
    Serial.println("RIGHT");
    if (!digitalRead(RIGHT)) {
      currStage++;
      Serial.print("correct");
    } else {
      Serial.print("wrong");
      while (digitalRead(CENTER)) {
        delay(1);
      }
    }
  }
  else if (randNumber == 3) {
    Serial.println("LEFT");
    if (!digitalRead(LEFT)) {
      Serial.print("correct");
      currStage++;
    } else {
      Serial.print("wrong");
      while (digitalRead(CENTER)) {
        delay(1);
      }
    }
  }
  else {
    while (digitalRead(CENTER)) {
      delay(1);
  }
}

void loop() {
  tempo = analogReadMilliVolts(POT);

  dip[0] = digitalRead(DIP1); 
  dip[1] = digitalRead(DIP2); 
  dip[2] = digitalRead(DIP3); 
  dip[3] = digitalRead(DIP4); 
  dip[4] = digitalRead(DIP5); 
  dip[5] = digitalRead(DIP6); 
  dip[6] = digitalRead(DIP7); 
  dip[7] = digitalRead(DIP8); 

  numTicks++; 

  if (numTicks < 150) {
    if (numTicks == 0) {
      tone(SPEAKER, tempo, 1000);
    }
    if (read_input() == true) {
      currStage++;
      // async function to display green led light for 50 ticks and then switch color back to red then exit the function
    }
  } else if (numTicks < 250) {
    // switch color to yellow for the entire duration
    if (read_input() == true) {
      currStage++;
      // async function to display green led light for 50 ticks and then switch color back to red then exit the function
    }
  } else {
    // lose condition
    // pause game
    Serial.println("You Lose\n Press Center to play again");
    while (!digitalRead(CENTER)) {
      delay(// whatever the tick is);
    }
    currStage = 0; 
    numTicks = 0; 
  }

  if (numTicks >= 250) {
    numTicks = 0;
  }

  delay(whatever this is)
  

  switch (currStage) {
    case 0:
      currStage++;
      if (dip[0]) {
        read_input()
      }
      } else {
        // speaker beeps instead
      }
      break;

      case 2:
  }
  for (int i = 0; i < 8; i++) {
    if (dip[i]) {
      randNumber = random(4);
      
      
    }
    delay(tempo);
  }

  

  
  }


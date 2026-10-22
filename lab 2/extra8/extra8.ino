#include <HardwareSerial.h>
#include <SPI.h>

#define LEDGREEN 45
#define LEDRED 48

SPIClass *vspi = NULL;
HardwareSerial ReceiverSerial(1);

int receivedValue = 0;
bool valueComplete = false;
int code;

unsigned long lastCall = 0;
const unsigned long interval = 30UL * 60UL * 1000UL; // 30 minutes in ms
static const int spiClk = 1000000; 

void randomCode(){
  code = random(1000, 10000); // upper bound is exclusive
}

void spiCommand(SPIClass *spi, uint16_t data) {
  //use it as you would the regular arduino SPI API
  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(spi->pinSS(), LOW);  //pull SS slow to prep other end for transfer
  spi->transfer16(data);
  digitalWrite(spi->pinSS(), HIGH);  //pull ss high to signify end of data transfer
  spi->endTransaction();
}

void setup() {
  // put your setup code here, to run once:
  delay(1000);
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Inside Setup");
  ReceiverSerial.begin(9600, SERIAL_8N1, 18, 17);
  randomCode();

  vspi = new SPIClass(SPI);
  vspi->begin(4,5,6,7); 

  pinMode(LEDGREEN, OUTPUT);
  pinMode(LEDRED, OUTPUT);

  digitalWrite(LEDRED, HIGH);
  Serial.printf("code: %d", code);

  lastCall = millis(); // start timer

}

void readSerialAsInt() {
  while (ReceiverSerial.available()) {
    char inChar = (char)ReceiverSerial.read();

    if (inChar == '\n') {
      valueComplete = true;
    } else if (inChar >= '0' && inChar <= '9') {
      // shift left by one digit and add new digit
      receivedValue = receivedValue * 10 + (inChar - '0');
    }
    // ignore any non-digit characters (like \r)
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  spiCommand(vspi, code);
  if (valueComplete) {
    Serial.print("Parsed int: ");
    Serial.println(receivedValue);
    if(receivedValue == code){
      digitalWrite(LEDRED, LOW);
      digitalWrite(LEDGREEN, HIGH);
      delay(5000);
    }
    else{
      Serial.printf("not the right code, expected: %d, got: %d", code, receivedValue);
    }
    // reset for next number
    receivedValue = 0;
    valueComplete = false;
  }
  if (millis() - lastCall >= interval) {
    randomCode();
    lastCall = millis(); // reset timer
  }
  readSerialAsInt();
}
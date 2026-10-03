#include "Wire.h"
#include "ICM_20948.h"

#define SERIAL_PORT Serial
#define WIRE_PORT Wire

#define AD0_VAL 1
ICM_20948_I2C myICM;
#define S3_ADDRESS 0x08

#define FORWARD  0x01
#define BACKWARD 0x02
#define LEFT     0x04
#define RIGHT    0x08
#define UP       0x10
#define DOWN     0x20

void setup() {
  // put your setup code here, to run once:
  SERIAL_PORT.begin(115200);
  WIRE_PORT.begin(22,21);   //SDA, SCL
  WIRE_PORT.setClock(400000);
  
  bool initialized = false;
  while(!initialized)
  {
    myICM.begin(WIRE_PORT, AD0_VAL);
    SERIAL_PORT.print(F("Initialization of sensor returned: "));
    SERIAL_PORT.println(myICM.statusString());
    if (myICM.status != ICM_20948_Stat_Ok) {
      SERIAL_PORT.println("Trying again...");
    }
    else {
      initialized = true;
    }
  }
}

float accX;
float accY;
float accZ;

void loop() {
  if(myICM.dataReady()) {
    myICM.getAGMT();
    printScaledAGMT(&myICM);
    accX = printFormattedFloat(myICM.accX(), 5, 2); // for forward and backward
    accY = printFormattedFloat(myICM.accY(), 5, 2); // for left and right
    accZ = printFormattedFloat(myICM.accZ(), 5, 2); // for up and down
    delay(300);
  } else {
    SERIAL_PORT.println("Waiting for data");
    delay(500);
  }
  if (accX > 500) {
    Serial.println("forward");
    send_message(FORWARD);
  } 
  else if (accX < -500) {
    Serial.println("backward");
    send_message(BACKWARD);
  }
  else if (accY > 500) {
    Serial.println("left");
    send_message(LEFT);
  }
  else if (accY < -500) {
    Serial.println("right");
    send_message(RIGHT);
  }
  else if (accZ < 900 && (accX > -500 && accX < 500) && (accY > -500 && accY < 500)) {
    Serial.println("up");
    send_message(UP);
  } 
  else if (accZ > 1100) {
    Serial.println("down");
    send_message(DOWN);
  }
  else {
    Serial.println("nothing");
  }

}

void printScaledAGMT(ICM_20948_I2C *sensor)
{
  SERIAL_PORT.print("Scaled. Acc (mg) [ ");
  printFormattedFloat(sensor->accX(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->accY(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->accZ(), 5, 2);
  SERIAL_PORT.print(" ], Gyr (DPS) [ ");
  printFormattedFloat(sensor->gyrX(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->gyrY(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->gyrZ(), 5, 2);
  SERIAL_PORT.print(" ], Mag (uT) [ ");
  printFormattedFloat(sensor->magX(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->magY(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->magZ(), 5, 2);
  SERIAL_PORT.print(" ], Tmp (C) [ ");
  printFormattedFloat(sensor->temp(), 5, 2);
  SERIAL_PORT.print(" ]");
  SERIAL_PORT.println();
}

float printFormattedFloat(float val, uint8_t leading, uint8_t decimals)
{
  float aval = abs(val);
  if (val < 0)
  {
    SERIAL_PORT.print("-");
  }
  else
  {
    SERIAL_PORT.print(" ");
  }
  for (uint8_t indi = 0; indi < leading; indi++)
  {
    uint32_t tenpow = 0;
    if (indi < (leading - 1))
    {
      tenpow = 1;
    }
    for (uint8_t c = 0; c < (leading - 1 - indi); c++)
    {
      tenpow *= 10;
    }
    if (aval < tenpow)
    {
      SERIAL_PORT.print("0");
    }
    else
    {
      break;
    }
  }
  if (decimals > 0)
  {
    SERIAL_PORT.print(aval, decimals);
  }
  else
  {
    SERIAL_PORT.print((uint32_t)aval);
  }
  return val;
}

void send_message(uint8_t data) {
  Wire.beginTransmission(S3_ADDRESS);
  Wire.write(data);
  Wire.endTransmission();
}

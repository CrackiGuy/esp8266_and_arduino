#include <IRremote.h>
#include <Wire.h>
#include <Average.h>

Average<float> ave1(4);
Average<float> ave2(4);

/*
 Sharp Optical Dust Sensor GP2Y1010AU0F
*/

// ------------------------------------------
int measurePin = A6; //Connect dust sensor to Arduino A6 pin
int ledPower = 2;   //Connect 3 led driver pins of dust sensor to Arduino D2

int moisturePin = A2;

int IR_receive_recv_PIN = 6;
int IR_receive_GND_PIN  = 7;

int IR_send_GND_PIN  = 12;


// A6  : DUST IN
// D2  : DUST OUT
//
// D3  : IR OUT
//
// D6  : IR IN
// D7  : OUT GND
//
// D12 : OUT GND

/* apple remote

menu  : 77E1403C
up    : 77E1D03C
down  : 77E1B03C
forward : 77E1E03C
reverse : 77E1103C
play  : 77E1203C


*/

IRrecv irrecv(IR_receive_recv_PIN); // Receive on pin 6
IRsend irsend;

// ------------------------------------------
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

int voMeasured = 0;
int moMeasured = 0;

float calcVoltage = 0;
float dustDensity = 0;

// -----------------------------------------
unsigned long startMills;
volatile unsigned long ac_startMills;
// ------------------------------------------
// IR
const int AC_TYPE  = 0;
// 0 : TOWER
// 1 : WALL

volatile int AC_POWER_ON    = 0;
// 0 : off
// 1 : on

int AC_AIR_ACLEAN  = 0;
// 0 : off
// 1 : on --> power on

volatile int AC_TEMPERATURE = 27;
// temperature : 18 ~ 30

volatile int AC_FLOW        = 0;
// 0 : low
// 1 : mid
// 2 : high

// IR
const int AC_FLOW_TOWER[3] = {0, 4, 6};
const int AC_FLOW_WALL[3]  = {0, 2, 4};

unsigned long AC_CODE_TO_SEND;

int r = LOW;
int o_r = LOW;

byte a, b;

// IR
volatile int sleepmode = LOW ;
volatile int o_sleepmode = HIGH ;

// ------------------------------------------
// RGB for ICMP display pin
const int RGBLED[6] = {4, 5, 8, 9, 10,11};

//

// IR
void ac_send_code(unsigned long code)
{
  Serial.print("code to send : ");
  //  Serial.print(code, BIN);
  //  Serial.print(" : ");
  Serial.println(code, HEX);

  delay(100);
  irsend.sendLG(code, 28);

  delay(100);
  irrecv.enableIRIn(); // Start the receiver
}

void ac_activate(int temperature, int air_flow)
{

  int AC_MSBITS1 = 8;
  int AC_MSBITS2 = 8;
  int AC_MSBITS3 = 0;
  int AC_MSBITS4 = 0;
  int AC_MSBITS5 = temperature - 15;
  int AC_MSBITS6 ;

  if ( AC_TYPE == 0) {
    AC_MSBITS6 = AC_FLOW_TOWER[air_flow];
  } else {
    AC_MSBITS6 = AC_FLOW_WALL[air_flow];
  }

  int AC_MSBITS7 = (AC_MSBITS3 + AC_MSBITS4 + AC_MSBITS5 + AC_MSBITS6) & B00001111;

  AC_CODE_TO_SEND =  AC_MSBITS1 << 4 ;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS2) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS3) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS4) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS5) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS6) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS7);

  ac_send_code(AC_CODE_TO_SEND);

  AC_POWER_ON = 1;
  AC_TEMPERATURE = temperature;
  AC_FLOW = air_flow;
}

void ac_change_air_swing(int air_swing)
{
  if ( AC_TYPE == 0) {
    if ( air_swing == 1) {
      AC_CODE_TO_SEND = 0x881316B;
    } else {
      AC_CODE_TO_SEND = 0x881317C;
    }
  } else {
    if ( air_swing == 1) {
      AC_CODE_TO_SEND = 0x8813149;
    } else {
      AC_CODE_TO_SEND = 0x881315A;
    }
  }

  ac_send_code(AC_CODE_TO_SEND);
}

void ac_power_down()
{
  AC_CODE_TO_SEND = 0x88C0051;

  ac_send_code(AC_CODE_TO_SEND);

  AC_POWER_ON = 0;
}

void ac_air_clean(int air_clean)
{
  if ( air_clean == 1) {
    AC_CODE_TO_SEND = 0x88C000C;
  } else {
    AC_CODE_TO_SEND = 0x88C0084;
  }

  ac_send_code(AC_CODE_TO_SEND);

  AC_AIR_ACLEAN = air_clean;
}

void ac_sleepmomde_change() {
  if ( AC_POWER_ON == 0 ) {
    ac_activate(AC_TEMPERATURE, AC_FLOW);
    Serial.println("CHANGE AC MODE : ON");
  } else {
    ac_power_down();
    Serial.println("CHANGE AC MODE : OFF");
  }
  //irrecv.enableIRIn(); // Start the receiver
}

// IR
void dumpInfo(decode_results *results)
{
  //Serial.println("======> IR code received");

  if ( results->bits > 0 && results->bits == 32 ) {
    switch (results->value) {
      // sleep mode start / stop
      case 0x77E1203C:
        ac_startMills = millis() + 1799900;
        Serial.print("IR -----> Sleep Mode Change ----->  ");
        Serial.println(!sleepmode);
        sleepmode = !sleepmode;
        delay(50);
        break;

      // ac power down
      case 0x77E1403C:
        Serial.println("IR -----> AC Power Down");
        ac_power_down();
        sleepmode = LOW;
        delay(50);
        break;

      // temp up
      case 0x77E1D03C:
        Serial.println("IR -----> temp up");
        if ( AC_TEMPERATURE < 30 ) {
          ac_activate((AC_TEMPERATURE + 1), AC_FLOW);
        }
        delay(50);
        break;

      // temp down
      case 0x77E1B03C:
        Serial.println("IR -----> temp down");
        if ( AC_TEMPERATURE > 18 ) {
          ac_activate((AC_TEMPERATURE - 1), AC_FLOW);
        }
        delay(50);
        break;

      // flow up
      case 0x77E1E03C:
        Serial.println("IR -----> flow up");
        if ( AC_FLOW < 2) {
          ac_activate(AC_TEMPERATURE, (AC_FLOW + 1));
        }
        delay(50);
        break;

      // flow down
      case 0x77E1103C:
        Serial.println("IR -----> flow down");
        if ( AC_FLOW > 0) {
          ac_activate(AC_TEMPERATURE, (AC_FLOW - 1));
        }
        delay(50);
        break;

    }
  }
  delay(50);
  irrecv.resume(); // Continue receiving
}

void setup()
{
  Serial.begin(38400);

  startMills = millis();
  ac_startMills = 0;

  Serial.println("Starting dust Sensor");
  pinMode(ledPower, OUTPUT);

  pinMode(IR_receive_GND_PIN, OUTPUT);
  pinMode(IR_send_GND_PIN, OUTPUT);

  digitalWrite(IR_receive_GND_PIN, LOW);
  digitalWrite(IR_send_GND_PIN, LOW);

  irrecv.enableIRIn(); // Start the receiver

  for ( int i = 0; i < 6 ; i++ )
  {
    pinMode(RGBLED[i], OUTPUT);
    digitalWrite(RGBLED[i], LOW);
  }

  Wire.begin(2);                // join i2c bus with address #2
  Wire.onRequest(requestEvent); // register event
  Wire.onReceive(receiveEvent);

}

void loop()
{

  decode_results results;

  if (irrecv.decode(&results)) {
    dumpInfo(&results);
  }

  if ((millis() - startMills) > 200 ) {

    digitalWrite(ledPower, LOW); // power on the LED
    delayMicroseconds(samplingTime);

    voMeasured = analogRead(measurePin); // read the dust value

    delayMicroseconds(deltaTime);
    digitalWrite(ledPower, HIGH); // turn the LED off
    delayMicroseconds(sleepTime);

    moMeasured = analogRead(moisturePin);

    ave1.push(voMeasured);
    ave2.push(moMeasured);

    startMills = millis();

    /*
    Serial.println(ave1.mean());
    Serial.println(ave2.mean());
    */
    
    for ( int i = 0; i < 6 ; i++ )
    {
      digitalWrite(RGBLED[i], LOW);
    }
  }

  if ((sleepmode == HIGH) && ( ( millis() - ac_startMills) >= 1800000 )) {
    ac_sleepmomde_change();
    ac_startMills = millis();
  }

  for ( int i = 0; i < 6 ; i++ )
  {
    digitalWrite(RGBLED[i], HIGH);
  }

  delay(10);

}

void requestEvent()
{
  byte myArray[4];
  int sleepmodetosend ;

  if ( sleepmode == o_sleepmode ) {
    myArray[0] = (int(ave1.mean()) >> 8 ) & 0xFF;
    myArray[1] = int(ave1.mean()) & 0xFF;
    myArray[2] = (int(ave2.mean()) >> 8 ) & 0xFF;
    myArray[3] = int(ave2.mean()) & 0xFF;

    Wire.write(myArray, 4);
  }

  if ( sleepmode != o_sleepmode )  {
    if ( sleepmode == HIGH ) {
      sleepmodetosend = 33333 ;
    } else {
      sleepmodetosend = 22222 ;
    }
    myArray[0] = (sleepmodetosend >> 8 ) & 0xFF;
    myArray[1] = sleepmodetosend & 0xFF;
    myArray[2] = (int(ave2.mean()) >> 8 ) & 0xFF;
    myArray[3] = int(ave2.mean()) & 0xFF;

    Wire.write(myArray, 4);

    o_sleepmode = sleepmode;
  }
}

void receiveEvent(int howMany)
{
  a = Wire.read();
  b = Wire.read();
  r = !r ;
}


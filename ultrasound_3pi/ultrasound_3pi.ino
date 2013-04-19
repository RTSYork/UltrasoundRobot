#include <OrangutanAnalog.h>
#include <OrangutanBuzzer.h>
//#include <OrangutanLCD.h>
#include <OrangutanLEDs.h>
#include <OrangutanMotors.h>
//#include <OrangutanPushbuttons.h>

#include <avr/pgmspace.h>

//Settings
const unsigned int pidInterval = 100; //ms
const unsigned int odoInterval = 100; //ms
const float pidP = 0.08;
const float pidI = 0.001;
const float pidD = 0.002;
const float axelLength = 81.5; //mm
const float wheelDiameter = 31.32; //mm
const float wheelCircumference = wheelDiameter * PI; //mm
const unsigned char encoderClicksPerRev = 30; //Number of encoder change interrupts per wheel rev
const unsigned long maxEncoderTimeDiff = 400000; //uS

//Definitions - Buttons
#define BUTTON_A (1 << PORTB1)
#define BUTTON_B (1 << PORTB4)
#define BUTTON_C (1 << PORTB5)

//Definitions - Encoders
#define ENCODER_LEFT (1 << PORTD2)
#define ENCODER_RIGHT (1 << PORTD4)

//Definitions - Encoder conversions
#define encoderPeriodToSpeed(x) (1000000 / ((float) (x))) / encoderClicksPerRev * wheelCircumference
#define encoderCountToDist(x) ((float) (x)) / encoderClicksPerRev * wheelCircumference

//PID control state structure
struct PID_STATE {   
  double integral;
  double errorPrev;
};

//Tunes
const char tune1[] PROGMEM = ">g32>>c32";
const char tune2[] PROGMEM = "L16 cdegreg4";
const char tune3[] PROGMEM = "o7l16crc";

//Variables - button press state
volatile unsigned long buttonTimePrev = 0;
volatile unsigned char buttonPress = 0;

//Variables - encoder pin state and click counts
volatile unsigned char encOldPinState = 0;
volatile unsigned long encLeftCount = 0;
volatile unsigned long encRightCount = 0;
volatile unsigned long prevEncLeftTime = 0;
volatile unsigned long prevEncRightTime = 0;
volatile unsigned long encLeftTimeAvg = 0;
volatile unsigned long encRightTimeAvg = 0;

//Variables - odometry
unsigned long nextOdoTime = 0;
unsigned long odoLeftCountPrev = 0;
unsigned long odoRightCountPrev = 0;
float currentX = 0;
float currentY = 0;
float currentTheta = 0;

//Variables - motor
unsigned char motorDirLeft = 0;
unsigned char motorDirRight = 0;
unsigned char motorSpeedLeft = 0;
unsigned char motorSpeedRight = 0;

//Variables - PID timer and state
unsigned long pidNextTime = 0;
unsigned char pidEnabled = 0;
unsigned int targetLeft = 0;
unsigned int targetRight = 0;
struct PID_STATE pidLeft;
struct PID_STATE pidRight;

//Variables - misc
unsigned char boolDebugEnabled = 0;

void setup() {
  //Init serial
  Serial.begin(115200);

  //Send welcome message
  Serial.println("3PI!");

  //Init buttons, enable pull-up resistors and interrupts
  PORTB |= BUTTON_A | BUTTON_B | BUTTON_C;
  PCICR |= _BV(PCIE0);
  PCMSK0 |= BUTTON_A | BUTTON_B | BUTTON_C;

  //Init encoders, enable interrupts
  PCICR |= _BV(PCIE2);
  PCMSK2 |= ENCODER_LEFT | ENCODER_RIGHT;

  //Enable interrupts
  sei();  

  //Play welcome tune
  OrangutanBuzzer::playFromProgramSpace(tune1);
}

//Pin change interrupt (PB0 - PB7) - buttons
ISR(PCINT0_vect) {
  //Get time
  unsigned long now = millis();

  //Check time
  if(now - buttonTimePrev > 10) {
    //Update button state
    buttonPress |= (~PINB & BUTTON_A) | (~PINB & BUTTON_B) | (~PINB & BUTTON_C);

    //Update last accepted press time
    buttonTimePrev = now;
  }
}

ISR(PCINT2_vect) {
  //Get new state
  unsigned char encNewPinState = PIND;

  //Get current time
  unsigned long now = micros();

  switch(encOldPinState ^ encNewPinState) {
  case ENCODER_LEFT: 
    {
      //Increment left count
      encLeftCount++;

      //Compute time difference
      unsigned long diff = now - prevEncLeftTime;
      prevEncLeftTime = now;

      //Update average
      if(diff < maxEncoderTimeDiff) {
        //Check wheels are currently moving
        if(encLeftTimeAvg != 0) {
          //Update average
          encLeftTimeAvg = (encLeftTimeAvg * 3 + diff) >> 2;
        } 
        else {
          //Start average
          encLeftTimeAvg = diff;
        }
      } 
      else {
        //Reset average
        encLeftTimeAvg = 0;
      }

      break;
    }
  case ENCODER_RIGHT: 
    {
      //Increment right count
      encRightCount++;

      //Compute time difference
      unsigned long diff = now - prevEncRightTime;
      prevEncRightTime = now;

      //Update average
      if(diff < maxEncoderTimeDiff) {
        //Check wheels are currently moving
        if(encRightTimeAvg != 0) {
          //Update average
          encRightTimeAvg = (encRightTimeAvg * 3 + diff) >> 2;
        } 
        else {
          //Start average
          encRightTimeAvg = diff;
        }
      } 
      else {
        //Reset average
        encRightTimeAvg = 0;
      }

      break;
    }
  }

  //Save new state
  encOldPinState = encNewPinState;
}

//Main loop
void loop() {  
  processSerial();
  processOdometry();
  processPID();
}

//Handle serial comms
void processSerial() {
  //Check for serial data
  if (Serial.available() > 0) {
    //Get command byte
    unsigned char command = Serial.read();

    //Act on command
    switch(command) {
    case 0x01: 
      {
        //Set pid state
        while(Serial.available() < 1);
        pidEnabled = Serial.read();

        //Check pid enabled
        if(!pidEnabled) {
          //Disable motors
          motorSpeedLeft = 0;
          motorSpeedRight = 0;

          //Set motor speeds
          OrangutanMotors::setSpeeds(motorDirLeft * motorSpeedLeft, motorDirRight * motorSpeedRight);
        }

        break;
      }
    case 0x02: 
      {
        //Set pid target speeds
        while(Serial.available() < 4);
        int tmpTargetLeft = (int) (((unsigned int) Serial.read() << 8) | (unsigned int) Serial.read());
        int tmpTargetRight = (int) (((unsigned int) Serial.read() << 8) | (unsigned int) Serial.read());

        //Split direction and speed from target - left
        if(tmpTargetLeft < 0) {
          //Set direction
          motorDirLeft = -1;

          //Set speed
          targetLeft = (0 - tmpTargetLeft);
        } 
        else {
          //Set direction
          motorDirLeft = 1;

          //Set speed
          targetLeft = tmpTargetLeft;
        }

        //Split direction and speed from target - right
        if(tmpTargetRight < 0) {
          //Set direction
          motorDirRight = -1;

          //Set speed
          targetRight = (0 - tmpTargetRight);
        } 
        else {
          //Set direction
          motorDirRight = 1;

          //Set speed
          targetRight = tmpTargetRight;
        }

        //Output debug info
        if(boolDebugEnabled) {
          Serial.print("DBG: TSPD: ");
          Serial.print(targetLeft, DEC);
          Serial.print(" # ");
          Serial.println(targetRight, DEC);
        }

        break;
      }
    case 0x03: 
      {
        //Set motor speed
        while(Serial.available() < 3);
        unsigned char tmpDir = Serial.read();
        motorDirLeft = (tmpDir & 0x01 ? -1 : 1);
        motorDirRight = (tmpDir & 0x02 ? -1 : 1);
        motorSpeedLeft = (unsigned char) Serial.read();
        motorSpeedRight = (unsigned char) Serial.read();

        //Set motor speeds
        OrangutanMotors::setSpeeds(motorDirLeft * motorSpeedLeft, motorDirRight * motorSpeedRight);

        //Output debug info
        if(boolDebugEnabled) {
          Serial.print("DBG: SPD: ");
          Serial.print(motorSpeedLeft, DEC);
          Serial.print(" # ");
          Serial.println(motorSpeedRight, DEC);
        }

        break;
      }
    case 0x04:
    {
      //Request position
      Serial.write((uint8_t*) &currentX, sizeof(currentX));
      Serial.write((uint8_t*) &currentY, sizeof(currentY));
      Serial.write((uint8_t*) &currentTheta, sizeof(currentTheta));
      
      break;
    }
    case 0xfe: 
      {
        //Beep
        OrangutanBuzzer::playNote(NOTE_A(5), 100, 15);
        break;
      }
    case 0xff:
      {
        //Set debugging state
        while(Serial.available() < 1);
        boolDebugEnabled = Serial.read();

        //Output debug info
        if(boolDebugEnabled) {
          Serial.println("DBG: Enabled");
        } 
        else {
          Serial.println("DBG: Disabled");
        }

        break;
      }
    default:
      //Bad command
      OrangutanBuzzer::playFromProgramSpace(tune3);

      break;
    }
  }

  //Check button state
  if(buttonPress) {
    //Copy and reset button state
    unsigned char tmpButtonPress = buttonPress;
    buttonPress = 0;

    //Send button press
    Serial.write(0x01);
    Serial.write(tmpButtonPress);
  }
}

void processOdometry() {
  //Check odometry timer
  if(millis() > nextOdoTime) {
    //Read encoder counts
    unsigned long tmpEncLeftCount = readISRULong(&encLeftCount);
    unsigned long tmpEncRightCount = readISRULong(&encRightCount);

    //Compute count differences
    unsigned long encLeftCountDiff = tmpEncLeftCount - odoLeftCountPrev;
    unsigned long encRightCountDiff = tmpEncRightCount - odoRightCountPrev;

    //Update encoder previous count values
    odoLeftCountPrev = tmpEncLeftCount;
    odoRightCountPrev = tmpEncRightCount;

    //Compute distances
    float encLeftDist = encoderCountToDist(encLeftCountDiff);
    float encRightDist = encoderCountToDist(encRightCountDiff);

    //Compute current sine and cosine values
    float currSin = sin(currentTheta);
    float currCos = cos(currentTheta);

    //Compare left and right differences
    if(encLeftCountDiff == encRightCountDiff) {
      //Moving in a straight line
      currentX += encLeftDist * currCos;
      currentY += encLeftDist * currSin;
    } 
    else {
      //Moving in an arc
      float expr1 = axelLength * (encRightDist + encLeftDist) / 2 / (encRightDist - encLeftDist);

      float rightMinusLeft = (encRightDist - encLeftDist);

      currentX += expr1 * (sin(rightMinusLeft / axelLength + currentTheta) - currSin);
      currentY += expr1 * (cos(rightMinusLeft / axelLength + currentTheta) - currCos);

      currentTheta += rightMinusLeft / axelLength;

      while(currentTheta > PI) currentTheta -= (2 * PI);
      while(currentTheta < -PI) currentTheta += (2 * PI);
    }

    //Calculate next odo time
    nextOdoTime += odoInterval;
  }

  //Output debug info
  if(boolDebugEnabled && millis() % 500 == 0) {
    Serial.print("DBG: POS: ");
    Serial.print(currentX, DEC); 
    Serial.print(" # ");
    Serial.print(currentY, DEC); 
    Serial.print(" # ");
    Serial.println(currentTheta, DEC); 
  }
}

void processPID() {
  //Check PID timer
  if(millis() >= pidNextTime) {
    //Check PID control enabled
    if(pidEnabled) {
      //Read encoder time averages
      unsigned long tmpEncLeft = readISRULong(&encLeftTimeAvg);
      unsigned long tmpEncRight = readISRULong(&encRightTimeAvg);

      //Read last encoder input times
      unsigned long tmpPrevEncLeftTime = readISRULong(&prevEncLeftTime);
      unsigned long tmpPrevEncRightTime = readISRULong(&prevEncRightTime);

      //Get time
      unsigned long now = micros();

      //Check wheels are rotating
      if(now - tmpPrevEncLeftTime >= maxEncoderTimeDiff) tmpEncLeft = 0;
      if(now - tmpPrevEncRightTime >= maxEncoderTimeDiff) tmpEncRight = 0;

      //Convert encoder period in uS to wheel speed in mm/s
      float tmpEncLeftSpeed = (tmpEncLeft != 0 ? encoderPeriodToSpeed(tmpEncLeft) : 0);
      float tmpEncRightSpeed = (tmpEncRight != 0 ? encoderPeriodToSpeed(tmpEncRight) : 0);

      //Compute change in speed via PID algorithm
      float speedChangeLeft = updatePID(&pidLeft, targetLeft, tmpEncLeftSpeed);
      float speedChangeRight = updatePID(&pidRight, targetRight, tmpEncRightSpeed);

      //Compute updated wheel speeds
      int motorSpeedLeftNew = motorSpeedLeft + speedChangeLeft;
      int motorSpeedRightNew = motorSpeedRight + speedChangeRight;

      //Limit motor speeds
      if(motorSpeedLeftNew < 0) motorSpeedLeftNew = 0;
      if(motorSpeedLeftNew > 255) motorSpeedLeftNew = 255;
      if(motorSpeedRightNew < 0) motorSpeedRightNew = 0;
      if(motorSpeedRightNew > 255) motorSpeedRightNew = 255;      

      //Encorporate and smooth new wheel speeds
      motorSpeedLeft = (motorSpeedLeft * 3 + motorSpeedLeftNew) / 4;
      motorSpeedRight = (motorSpeedRight * 3 + motorSpeedRightNew) / 4;

      //Ensure motors are disabled when wheels are stopped
      if(targetLeft == 0 && motorSpeedLeft < 10) motorSpeedLeft = 0;
      if(targetRight == 0 && motorSpeedRight < 10) motorSpeedRight = 0;

      //Set wheel speeds
      OrangutanMotors::setSpeeds(motorDirLeft * motorSpeedLeft, motorDirRight * motorSpeedRight);

      //Output debug info
      if(boolDebugEnabled && millis() % 250 == 0) {
        Serial.print("DBG: PID: ");
        Serial.print(tmpEncLeftSpeed, DEC); 
        Serial.print(" # ");
        Serial.print(tmpEncRightSpeed, DEC); 
        Serial.print(" -> ");
        Serial.print(speedChangeLeft, DEC); 
        Serial.print(" # ");
        Serial.print(speedChangeRight, DEC); 
        Serial.print(" -> ");
        Serial.print(motorSpeedLeft, DEC); 
        Serial.print(" # ");
        Serial.println(motorSpeedRight, DEC); 
      }
    }

    //Calculate next PID time
    pidNextTime += pidInterval;
  }
}

//Update pid state
float updatePID(struct PID_STATE* pidState, float setPoint, float measuredVal) {
  //Compute error
  float error = setPoint - measuredVal;

  //Compute integral
  pidState->integral += error;

  //Compute derivative
  float derivative = (float) (error - pidState->errorPrev);

  //Update previous error
  pidState->errorPrev = error;

  //Compute output
  return (pidP * error) + (pidI * pidState->integral) + (pidD * derivative);
}

//Retrieve ISR shared variable, checking it doesn't change part way through
unsigned long readISRULong(volatile unsigned long* ptr) {
  unsigned long tmp;

  //Read value until it doesn't change
  do {
    tmp = *ptr;
  } 
  while(tmp != *ptr);

  return tmp;
}



//#include <OrangutanAnalog.h>
#include <OrangutanBuzzer.h>
//#include <OrangutanLCD.h>
//#include <OrangutanLEDs.h>
#include <OrangutanMotors.h>
//#include <OrangutanPushbuttons.h>

#include <avr/pgmspace.h>

// Stop pesky deprecated string constants warnings
#pragma GCC diagnostic ignored "-Wwrite-strings"

// Workaround for gcc bug
#undef PROGMEM
#define PROGMEM __attribute__((section(".progmem.data")))

// Settings
const unsigned long odoInterval = 100; // ms
const unsigned long motInterval = 100; // ms
const unsigned long odoDebugInterval = 500; // ms
const unsigned long motDebugInterval = 500; // ms
const unsigned long btnDebounceInterval = 10; // ms

const double pidP = 1.0;
const double pidI = 0.00;
const double pidD = 0.2;

const double wheelSeparation = 82; // mm
const double wheelDiameter = 32.5; // mm

const unsigned char encoderClicksPerRev = 30; // Number of encoder change interrupts per wheel rev

// Definitions - Buttons
#define BUTTON_A (1 << PORTB1)
#define BUTTON_B (1 << PORTB4)
#define BUTTON_C (1 << PORTB5)

// Definitions - Encoders
#define ENCODER_LEFT (1 << PORTD2)
#define ENCODER_RIGHT (1 << PORTD4)

// Definitions - Encoder conversions
const double wheelCircumference = wheelDiameter * PI; // mm
#define encoderCountToDist(x) (((double) (x)) * wheelCircumference) / encoderClicksPerRev
#define distToEncoderCount(x) (((double) (x)) * encoderClicksPerRev) / wheelCircumference

// Defintions - Angle conversions
#define radiansToDegrees(x) ((double) (x)) * (180.0 / PI)
#define degreesToRadians(x) (((double) (x)) * PI) / 180.0

// Definitions - Motor speeds
#define setMotorSpeeds() OrangutanMotors::setSpeeds((motorDir & 0x02 ? -1 : 1) * motorSpeedLeft, (motorDir & 0x01 ? -1 : 1) * motorSpeedRight)

// Definitions - Commands
enum commands {
  CMD_NONE = -1, // No command
  CMD_SET_DEBUG = 0x01, // Enable / disable debugging mode
  CMD_SET_MODE = 0x02, // Set platform mode (manual / automatic)
  CMD_SET_MOTOR_SPD = 0x03, // Set motor speeds (indivual in manual mode / maximum in automatic mode)
  CMD_SET_POS = 0x04, // Set target coordinates
  CMD_GET_POS = 0x05, // Get current coordinates
  CMD_BEEP = 0x06 // Emit beep
};

// Definitions - Command responses
enum responses {
  RESP_OK = 0x01, // Command succeeded
  RESP_ERR = 0x02, // Command failed
  RESP_POS = 0x03, // Position update
  RESP_BTN = 0x04 // Button press
};

// Definitions - Operating mode
enum operating_mode {
  MODE_MANUAL,
  MODE_AUTOMATIC,
};

// Defintions - Motion stages
enum motion_state {
  MOTION_WAITING,
  MOTION_READY,
  MOTION_ROTATE_1,
  MOTION_TRAVEL,
  MOTION_ROTATE_2
};

// Position
struct POSITION {
  double X;
  double Y;
  double Theta;
};

// PID control state structure
struct PID_STATE {   
  double integral;
  double errorPrev;
};

// Tunes
const char tune1[] PROGMEM = ">g32>>c32";
const char tune2[] PROGMEM = "L16 cdegreg4";
const char tune3[] PROGMEM = "o7l16crc";

// Strings
const char debugS[] = "#DBG: ";

// Variables - general
operating_mode mode = MODE_MANUAL;
commands serialCmd = CMD_NONE;

// Variables - button press state
volatile unsigned long buttonTimePrev = 0;
volatile unsigned char buttonPress = 0;

// Variables - encoder pin state and click counts
volatile unsigned char encOldPinState = 0;
volatile unsigned long encCountLeft = 0;
volatile unsigned long encCountRight = 0;

// Variables - position and speed
struct POSITION odoCurrentPos;
struct POSITION motTargetPos;
unsigned char motMaxSpeed = 0;
unsigned char motorDir = 0; // Bit 1 = Right Reverse, Bit 0 = Left Reverse
unsigned char motorSpeedLeft = 0;
unsigned char motorSpeedRight = 0;

// Variables - odometry
unsigned long odoNextTime = 0;
unsigned long odoNextDebugTime = 0;
unsigned long odoCountPrevLeft = 0;
unsigned long odoCountPrevRight = 0;

// Variables - motion control
unsigned long motNextTime = 0;
unsigned long motNextDebugTime = 0;
unsigned long motCountPrevLeft = 0;
unsigned long motCountPrevRight = 0;
unsigned long motCountTargetLeft = 0;
unsigned long motCountTargetRight = 0;
motion_state motStage = MOTION_WAITING;
struct PID_STATE motPIDState;

// Variables - misc
unsigned char boolDebugEnabled = 0;

void setup() {
  // Init serial
  Serial.begin(57600);

  // Send welcome message
  Serial.print("#3PI...");

  // Init buttons, enable pull-up resistors and interrupts
  PORTB |= BUTTON_A | BUTTON_B | BUTTON_C;
  PCICR |= _BV(PCIE0);
  PCMSK0 |= BUTTON_A | BUTTON_B | BUTTON_C;

  // Init encoders, enable interrupts
  PCICR |= _BV(PCIE2);
  PCMSK2 |= ENCODER_LEFT | ENCODER_RIGHT;

  // Enable interrupts
  sei();
  
  // Send welcome message
  Serial.println("Ready!");

  // Play welcome tune
  OrangutanBuzzer::playFromProgramSpace(tune1);
}

// Button interrupt
ISR(PCINT0_vect) {
  // Get time
  unsigned long now = millis();

  // Check time
  if(now - buttonTimePrev > btnDebounceInterval) {
    // Update button state
    buttonPress |= (~PINB & BUTTON_A) | (~PINB & BUTTON_B) | (~PINB & BUTTON_C);

    // Update last accepted press time
    buttonTimePrev = now;
  }
}

// Wheel encoder interrupt
ISR(PCINT2_vect) {
  // Get new state
  unsigned char encNewPinState = PIND;

  switch(encOldPinState ^ encNewPinState) {
  case ENCODER_LEFT: 
    {
      // Increment left count
      encCountLeft++;
      break;
    }
  case ENCODER_RIGHT: 
    {
      // Increment right count
      encCountRight++;
      break;
    }
  }

  // Save new state
  encOldPinState = encNewPinState;
}

// Main loop
void loop() {  
  processSerial();
  processOdometry();
  processMotion();
}

// Handle serial communications with FPGA
void processSerial() {
  // Check button state, get this out of the way before things get complicated
  if(buttonPress) {
    // Copy and reset button state
    unsigned char tmpButtonPress = buttonPress;
    buttonPress = 0;

    // Send button press
    Serial.write(RESP_BTN);
    Serial.write(tmpButtonPress);
  }
  
  // Attempt to read byte from UART if there is no command pending
  if(serialCmd == CMD_NONE && Serial.available() > 0) serialCmd = (commands) Serial.read();

  // Act on command
  switch(serialCmd) {
  case CMD_NONE: 
    {
      // Do nothing
      return;
    }
  case CMD_SET_DEBUG: 
    {
      // Set debugging state
      while(Serial.available() < 1) return;
      boolDebugEnabled = (unsigned char) Serial.read();

      // Output debug info
      debugPrint("DEBUG ", false);
      Serial.println(boolDebugEnabled ? "ENABLED" : "DISABLED");
      break;
    }
  case CMD_SET_MODE: 
    {
      // Set operating mode
      while(Serial.available() < 1) return;
      mode = (operating_mode) Serial.read();

      // Check which mode is begine entered
      switch(mode) {
      case MODE_MANUAL:
        {
          // Disable motors
          motorSpeedLeft = 0;
          motorSpeedRight = 0;

          // Set motor speeds
          setMotorSpeeds();

          // Output debug info
          debugPrint("MODE - MANUAL", true);
          break;
        }
      case MODE_AUTOMATIC:
        {
          // Force change motion state
          motStage = MOTION_WAITING;

          // Output debug info
          debugPrint("MODE - AUTO", true);
          break;
        }
      }
      break;
    }
  case CMD_SET_MOTOR_SPD: 
    {
      //  Wait for data
      while(Serial.available() < 3) return;
      
      // Set motor speed in manual mode or maximum speed in automatic mode
      switch(mode) {
      case MODE_MANUAL:
        {
          // Set motor speed
          motorDir = (unsigned char) Serial.read();
          motorSpeedLeft = (unsigned char) Serial.read();
          motorSpeedRight = (unsigned char) Serial.read();

          // Set motor speeds
          setMotorSpeeds();

          // Output debug info
          if(boolDebugEnabled) {
            debugPrint("MSPD: ", false);
            Serial.print(motorDir, DEC);
            Serial.print(", ");
            Serial.print(motorSpeedLeft, DEC);
            Serial.print(", ");
            Serial.println(motorSpeedRight, DEC);
          }
          break;
        }
      case MODE_AUTOMATIC:
        {
          motMaxSpeed = (unsigned char) Serial.read();
          Serial.read(); // Read additional bytes we dont care about
          Serial.read();

          // Output debug info
          if(boolDebugEnabled) {
            debugPrint("ASPD: ", false);
            Serial.println(motMaxSpeed, DEC);
          }
          break;
        }
      }
      break;
    }
  case CMD_SET_POS: 
    {
      // Set target position
      while(Serial.available() < 6) return;
      int data[3];
      unsigned char* dataPtr = (unsigned char*) &data;
      for(unsigned char i = 0; i < 6; i++) *dataPtr++ = Serial.read();
      
      // Store data
      motTargetPos.X = data[0];
      motTargetPos.Y = data[1];
      motTargetPos.Theta = degreesToRadians(data[2]);

      // Limit theta
      while(motTargetPos.Theta > PI) motTargetPos.Theta -= (2 * PI);
      while(motTargetPos.Theta < -PI) motTargetPos.Theta += (2 * PI);

      // Change motion state
      motStage = MOTION_READY;

      // Output debug info
      if(boolDebugEnabled) {
        debugPrint("TPOS: ", false);
        Serial.print(motTargetPos.X, DEC); 
        Serial.print(", ");
        Serial.print(motTargetPos.Y, DEC); 
        Serial.print(", ");
        Serial.println(radiansToDegrees(motTargetPos.Theta), DEC); 
      }
      break;
    }
  case CMD_GET_POS: 
    {
      // Get current position
      outputPosition();
      break;
    }
  case CMD_BEEP: 
    { 
      // Beep
      OrangutanBuzzer::playNote(NOTE_A(5), 100, 15);
      break;
    }
  default:
    {
      // Bad command
      OrangutanBuzzer::playFromProgramSpace(tune3);

      // Reset command
      serialCmd = CMD_NONE;
    
      // Send error response
      Serial.write(RESP_ERR);
    
      // Return early
      return;
    }
  }
  
  // Reset command
  serialCmd = CMD_NONE;
  
  // Send success message
  Serial.write(RESP_OK);
}

// Update current position using wheel encoders
void processOdometry() {
  // Check odometry timer
  if(millis() > odoNextTime) {
    // Read encoder counts
    unsigned long tmpEncCountLeft = readISRULong(&encCountLeft);
    unsigned long tmpEncCountRight = readISRULong(&encCountRight);

    // Compute count differences
    unsigned long encCountDiffLeft = tmpEncCountLeft - odoCountPrevLeft;
    unsigned long encCountDiffRight = tmpEncCountRight - odoCountPrevRight;

    // Update encoder previous count values
    odoCountPrevLeft = tmpEncCountLeft;
    odoCountPrevRight = tmpEncCountRight;

    // Compute distances - taking into account motor direction
    double encDistLeft = (motorDir & 0x02 ? -1.0 : 1.0) * encoderCountToDist(encCountDiffLeft);
    double encDistRight = (motorDir & 0x01 ? -1.0 : 1.0) * encoderCountToDist(encCountDiffRight);

    // Compute sbar
    double sBar = (encDistLeft + encDistRight) / 2;

    // Compute new theta
    odoCurrentPos.Theta = (encDistLeft - encDistRight) / wheelSeparation + odoCurrentPos.Theta;

    // Limit theta
    while(odoCurrentPos.Theta > PI) odoCurrentPos.Theta -= (2 * PI);
    while(odoCurrentPos.Theta < -PI) odoCurrentPos.Theta += (2 * PI);

    // Compute new X and Y
    odoCurrentPos.X = sBar * cos(odoCurrentPos.Theta) + odoCurrentPos.X;
    odoCurrentPos.Y = sBar * sin(odoCurrentPos.Theta) + odoCurrentPos.Y;

    // Compute next time
    odoNextTime += odoInterval;
  }

  // Output debug info
  if(boolDebugEnabled && millis() >= odoNextDebugTime) {
    // Dump info
    debugPrint("CPOS: ", false);
    Serial.print(odoCurrentPos.X, DEC); 
    Serial.print(", ");
    Serial.print(odoCurrentPos.Y, DEC); 
    Serial.print(", ");
    Serial.println(odoCurrentPos.Theta * (180 / PI), DEC);

    // Compute next time
    odoNextDebugTime += odoDebugInterval;
  }
}

// Process any motion request from FPGA
void processMotion() {
  // Check motion timer
  if(millis() >= motNextTime) {
    // Check motion control enabled
    if(mode == MODE_AUTOMATIC) {
      // Read encoder counts
      unsigned long tmpEncCountLeft = readISRULong(&encCountLeft);
      unsigned long tmpEncCountRight = readISRULong(&encCountRight);

      // Compute count differences
      unsigned long encCountDiffLeft = tmpEncCountLeft - motCountPrevLeft;
      unsigned long encCountDiffRight = tmpEncCountRight - motCountPrevRight;

      // Update encoder previous count values
      motCountPrevLeft = tmpEncCountLeft;
      motCountPrevRight = tmpEncCountRight;

      // Compute left and right wheel differences
      double diffLeftRight = (double) encCountDiffRight - (double) encCountDiffLeft;

      // Act according to stage of motion
      switch(motStage) {
      case MOTION_WAITING:
        {
          // Do nothing - no action required
          break;
        }
      case MOTION_READY:
        {
          // Compute angle between current position and target 
          double targetAngle = atan2(motTargetPos.Y - odoCurrentPos.Y, motTargetPos.X - odoCurrentPos.X);

          // Compute shortest angle between current heading and target heading
          double targetShortestAngle = targetAngle - odoCurrentPos.Theta;

          // Limit shortest angle
          while(targetShortestAngle > PI) targetShortestAngle -= (2 * PI);
          while(targetShortestAngle < -PI) targetShortestAngle += (2 * PI);

          // Compute distance travelled by each wheel in encoder clicks
          double targetClicks = round(distToEncoderCount((fabs(targetShortestAngle) * (wheelSeparation * PI)) / (2 * PI)));

          // Compute target count of each encoder          
          motCountTargetLeft = tmpEncCountLeft + (unsigned long) targetClicks;
          motCountTargetRight = tmpEncCountRight + (unsigned long) targetClicks;

          // Set motor directions
          if(targetShortestAngle < 0) {
            motorDir = 0x02;
          } 
          else {
            motorDir = 0x01;
          }

          // Change state
          motStage = MOTION_ROTATE_1;
          break;
        }
      case MOTION_ROTATE_1:
        {
          // Check if target heading reached
          if(tmpEncCountLeft >= motCountTargetLeft || tmpEncCountRight >= motCountTargetRight) {
            // Compute distance travelled by each wheel in encoder clicks
            double targetClicks = round(distToEncoderCount(sqrt(pow(motTargetPos.X - odoCurrentPos.X, 2) + pow(motTargetPos.Y - odoCurrentPos.Y, 2))));

            // Compute target count of each encoder          
            motCountTargetLeft = tmpEncCountLeft + (unsigned long) targetClicks;
            motCountTargetRight = tmpEncCountRight + (unsigned long) targetClicks;

            // Set motor directions
            motorDir = 0x00;

            // Change state
            motStage = MOTION_TRAVEL;
          }
          break;
        }
      case MOTION_TRAVEL:
        {
          // Check if target location reached, if not check that heading angle still correct
          if(tmpEncCountLeft >= motCountTargetLeft || tmpEncCountRight >= motCountTargetRight) {
            // Compute shortest angle between current heading and target heading
            double targetShortestAngle = motTargetPos.Theta - odoCurrentPos.Theta;

            // Limit shortest angle
            while(targetShortestAngle > PI) targetShortestAngle -= (2 * PI);
            while(targetShortestAngle < -PI) targetShortestAngle += (2 * PI);

            // Compute distance travelled by each wheel in encoder clicks
            double targetClicks = round(distToEncoderCount((fabs(targetShortestAngle) * (wheelSeparation * PI)) / (2 * PI)));

            // Compute target count of each encoder          
            motCountTargetLeft = tmpEncCountLeft + (unsigned long) targetClicks;
            motCountTargetRight = tmpEncCountRight + (unsigned long) targetClicks;

            // Set motor directions
            if(targetShortestAngle < 0) {
              motorDir = 0x02;
            } 
            else {
              motorDir = 0x01;
            }

            // Change state
            motStage = MOTION_ROTATE_2;
          }
          break;
        }
      case MOTION_ROTATE_2:
        {
          // Check if target heading reached
          if(tmpEncCountLeft >= motCountTargetLeft || tmpEncCountRight >= motCountTargetRight) {
            // Set motor directions
            motorDir = 0x00;
            
            // Output new position
            outputPosition();

            // Change state
            motStage = MOTION_WAITING;
          }
          break;
        }
      }

      // Compute speed difference via PID algorithm
      double pwrDiff = updatePID(&motPIDState, 0, diffLeftRight);

      // Limit power difference
      if(pwrDiff > motMaxSpeed) pwrDiff = motMaxSpeed;
      if(pwrDiff < -motMaxSpeed) pwrDiff = -motMaxSpeed;

      // Apply power difference to motor speeds if not idle, else stop motors
      if(motStage != MOTION_WAITING) {
        if(pwrDiff < 0) {
          motorSpeedLeft = motMaxSpeed;
          motorSpeedRight = (unsigned char) ((double) motMaxSpeed + pwrDiff);
        } 
        else {
          motorSpeedLeft = (unsigned char) ((double) motMaxSpeed - pwrDiff);
          motorSpeedRight = motMaxSpeed;
        }
      } 
      else {
        motorSpeedLeft = 0;
        motorSpeedRight = 0;
      }

      // Set motor speeds
      setMotorSpeeds();

      // Output debug info
      if(boolDebugEnabled && millis() >= motNextDebugTime) {
        // Dump info
        debugPrint("MOT: ", false);
        Serial.print(diffLeftRight, DEC); 
        Serial.print(" - ");
        Serial.print(pwrDiff, DEC); 
        Serial.print(" - ");
        Serial.print(motorSpeedLeft, DEC); 
        Serial.print(", ");
        Serial.println(motorSpeedRight, DEC);

        // Compute next time
        motNextDebugTime += motDebugInterval;
      }
    }

    // Calculate next time
    motNextTime += motInterval;
  }
}

// Update pid state
double updatePID(struct PID_STATE* pidState, double setPoint, double measuredVal) {
  // Compute error
  double error = setPoint - measuredVal;

  // Compute integral
  pidState->integral += error;

  // Compute derivative
  double derivative = (double) (error - pidState->errorPrev);

  // Update previous error
  pidState->errorPrev = error;

  // Compute output
  return (pidP * error) + (pidI * pidState->integral) + (pidD * derivative);
}

// Retrieve ISR shared variable, checking it doesn't change part way through
unsigned long readISRULong(volatile unsigned long* ptr) {
  unsigned long tmp;

  // Read value until it doesn't change
  do {
    tmp = *ptr;
  } while(tmp != *ptr);

  return tmp;
}

void outputPosition() {
  int data[3] = {odoCurrentPos.X, odoCurrentPos.Y, radiansToDegrees(odoCurrentPos.Theta)};
  unsigned char* dataPtr = (unsigned char*) &data;
  Serial.write(RESP_POS);
  for(unsigned char i = 0; i < 6; i++) Serial.write(*dataPtr++);
}

// Print formatted debugging information
void debugPrint(char* str, char newLine) {
  if(boolDebugEnabled) {
    Serial.print(debugS);
    if(newLine) {
      Serial.println(str);
    } else {
      Serial.print(str);
    }
  }
}

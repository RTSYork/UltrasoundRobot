#ifndef ULTRASOUND_H_
#define ULTRASOUND_H_

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xparameters.h"

#include "int_ctrl.h"
#include "uart.h"
#include "gpio.h"
#include "timer.h"
#include "usarray.h"
#include "us_receiver.h"
#include "mobplat.h"

// --------------------------------------------------------------------------------

// Definitions

// PC debugging commands
enum DEBUG_CMD {
	DEBUG_CMD_NONE = -1, // No command
	DEBUG_CMD_SET_DEBUG = 0x01, // Enable / disable debugging output
	DEBUG_CMD_SET_US_MODE = 0x02, // Set ultrasound array scan mode (single / complete)
	DEBUG_CMD_SET_US_SENSOR = 0x03, // Set ultrasound array sensor index
	DEBUG_CMD_SET_US_TRIGGERS = 0x04, // Set ultrasound array trigger levels
	DEBUG_CMD_SET_US_OUTPUT = 0x05, // Enable / disable ultrasound array data output
	DEBUG_CMD_ROBOT_COMMAND = 0x06, // Issue command to mobile platform
	DEBUG_CMD_PING = 0x07, // Issue ping command
	DEBUG_CMD_ROBOT_PASSTHROUGH = 0x08 // Enter robot passthrough mode, must reset to exit
};

// Ultrasound data output modes
enum US_OUTPUT {
	US_OUTPUT_NONE = 0x00, // Output off
	US_OUTPUT_WAVEFORM = 0x01, // Raw waveform
	US_OUTPUT_RANGE = 0x02 // Range data
};

// Mobile platform responses
enum PLATFORM_RESP {
	PLATFORM_RESP_NONE = -1, // No response
	PLATFORM_RESP_OK = 0x01, // Command succeeded
	PLATFORM_RESP_ERR = 0x02, // Command failed
	PLATFORM_RESP_POS = 0x03, // Position update
	PLATFORM_RESP_BTN = 0x04 // Button press
};

// Mobile platform position
struct POSITION {
	short X;
	short Y;
	short Theta;
};

// Mobile platform
#define MP_DEBUG_BUF_SIZE 128 // bytes

// Heartbeat
#define HEARTBEAT_INTERVAL 500 // ms

// --------------------------------------------------------------------------------

// Function prototypes

void ProcessSerialDebug();
void ProcessSerial3PI();
void ProcessUSArray();
void Passthrough3PI();
void TestFSL();
void Init3PI();
void Drive3PI();

void InterruptHandler_Timer_Sys(void *CallbackRef); // Increment system tick counter

void heartBeat(); // Flash heartbeat LED

void debugPrint(char* str, char newLine); // Print debugging message if debugging enabled

// --------------------------------------------------------------------------------

#endif /* ULTRASOUND_H_ */

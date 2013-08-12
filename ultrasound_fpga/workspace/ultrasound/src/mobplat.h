#ifndef MOBPLAT_H_
#define MOBPLAT_H_

#include "uart.h"

// Mobile platform commands
enum PLATFORM_CMD {
	PLATFORM_CMD_NONE = -1, // No command
	PLATFORM_CMD_SET_DEBUG = 0x01, // Enable / disable debugging mode
	PLATFORM_CMD_SET_MODE = 0x02, // Set platform mode (manual / automatic)
	PLATFORM_CMD_SET_MOTOR_SPD = 0x03, // Set motor speeds (indivual in manual mode / maximum in automatic mode)
	PLATFORM_CMD_SET_POS = 0x04, // Set target coordinates
	PLATFORM_CMD_GET_POS = 0x05, // Get current coordinates
	PLATFORM_CMD_BEEP = 0x06 // Emit beep
};

// Mobile platform directions
enum PLATFORM_DIR {
	PLATFORM_DIR_FORWARD = 0x00, // Both wheels forward
	PLATFORM_DIR_RIGHT = 0x01, // Spin right (left forward, right reverse)
	PLATFORM_DIR_LEFT = 0x02, // Spin left (right forward, left reverse)
	PLATFORM_DIR_REVERSE = 0x03 // Both wheels reverse
};

extern uart_buff UartBuffRobot; // UART connection between FPGA and 3PI

// Helpers to issue commands to mobile platform
void mpSetDebug(unsigned char state);
void mpSetMode(unsigned char mode);
void mpSetMotorSpeed(unsigned char dirMaxSpeed, unsigned char left, unsigned char right);
void mpSetPos(short X, short Y, short Theta);
void mpGetPos();
void mpBeep();

#endif /* MOBPLAT_H_ */

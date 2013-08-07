#include "mobplat.h"
#include "usarray.h"

uart_buff UartBuffRobot;

void mpSetDebug(unsigned char state) {
	// Send debug enable command
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_SET_DEBUG) == -1);
	while(uart_putchar(&UartBuffRobot, state) == -1);

	//int x = usRangeReadings[1];
}

void mpSetMode(unsigned char mode) {
	// Send platform set mode command
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_SET_MODE) == -1);
	while(uart_putchar(&UartBuffRobot, mode) == -1);
}

void mpSetMotorSpeed(unsigned char dirMaxSpeed, unsigned char left, unsigned char right) {
	// Send motor speeds (dirMaxSpeed is used as direction in manual mode or max speed in automatic mode where left and right are ignored)
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_SET_MOTOR_SPD) == -1);
	while(uart_putchar(&UartBuffRobot, dirMaxSpeed) == -1);
	while(uart_putchar(&UartBuffRobot, left) == -1);
	while(uart_putchar(&UartBuffRobot, right) == -1);
}

void mpSetPos(short X, short Y, short Theta) {
	// Send set position command
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_SET_POS) == -1);

	// Send data
	unsigned short data[3] = {X, Y, Theta};
	int i;
	unsigned char* dataPtr = (unsigned char*) &data;
	for(i = 0; i < 6; i++) while(uart_putchar(&UartBuffRobot, *dataPtr++) == -1);
}

void mpGetPos() {
	// Send get position command
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_GET_POS) == -1);
}

void mpBeep() {
	// Send beep command
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_BEEP) == -1);
}

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

// Helpers to issue commands to mobile platform
void mpSetDebug(unsigned char state);
void mpSetMode(unsigned char mode);
void mpSetMotorSpeed(unsigned char dirMaxSpeed, unsigned char left, unsigned char right);
void mpSetPos(short X, short Y, short Theta);
void mpGetPos();
void mpBeep();

void InterruptHandler_Timer_Sys(void *CallbackRef); // Increment system tick counter

void heartBeat(); // Flash heartbeat LED

void debugPrint(char* str, char newLine); // Print debugging message if debugging enabled

// --------------------------------------------------------------------------------

// Variables - devices
XIntc InterruptController; // Interrupt controller shared between all devices
uart_buff UartBuffDebug; // UART connection between PC and FPGA
uart_buff UartBuffRobot; // UART connection between FPGA and 3PI
gpio_state gpioDipSwitches; // GPIO for 4 on-board dip switches
gpio_state gpioLEDS; // GPIO for 4 on-board LEDS
gpio_state gpioUSDebug; // GPIO for ultrasound ADC end of conversion interrupt & debug IO
XTmrCtr TimerSys; // Timer for delays

// Variables - general
volatile u32 sysTickCounter = 0;

// Variables - debug
enum DEBUG_CMD debugCommand = DEBUG_CMD_NONE; // Pending debug command
char debugEnabled = 0x00; // Debug mode status
char debugRobotCmdSizeReceived = 0; // Debug mode, robot command length received

// Variables - mobile platform
enum PLATFORM_RESP mpResponse = PLATFORM_RESP_NONE;
char mpDebugParse = 0x00; // Currently parsing a debug message
char mpDebugBuf[MP_DEBUG_BUF_SIZE]; // Platform debug message buffer
unsigned char mpDebugIndex = 0; // Platform debug mess buffer pointer
struct POSITION mpCurrentPos; // Current platform position

// Variables - ultrasound array
char usarrayEnabled = 0x01; // Ultrasound array scanning status
enum US_OUTPUT usarrayOutputMode = US_OUTPUT_NONE; // Ultrasound array output to debug UART mode
char usarrayTempTaken = 0x00; // Ultrasound array temperate reading taken

// --------------------------------------------------------------------------------

int main() {
	// Get ready to rumble
	init_platform();

	// Startup message - printed directly to UART
	print("#Ultrasound...");

	// Init UARTS
	if(init_uart_buffers(XPAR_USB_UART_DEVICE_ID, &UartBuffDebug) != XST_SUCCESS) return XST_FAILURE;
	if(init_uart_buffers(XPAR_AXI_UARTLITE_3PI_DEVICE_ID, &UartBuffRobot) != XST_SUCCESS) return XST_FAILURE;

	// Init GPIO
	if(init_gpio(XPAR_DIP_SWITCHES_4BITS_DEVICE_ID, 0x0F, 0x00, &gpioDipSwitches) != XST_SUCCESS) return XST_FAILURE;
	if(init_gpio(XPAR_LEDS_4BITS_DEVICE_ID, 0x00, 0x00, &gpioLEDS) != XST_SUCCESS) return XST_FAILURE;

	// Init timer
	if(init_timer(XPAR_AXI_TIMER_0_DEVICE_ID, &TimerSys, XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ, 1000, (XTmrCtr_Handler) &InterruptHandler_Timer_Sys) != XST_SUCCESS) return XST_FAILURE;

	// Start system timer
	timer_setstate(&TimerSys, 1);

	// Init ultrasound array
	if(init_usarray() != XST_SUCCESS) return XST_FAILURE;

	// Init interrupt controller
	if(init_interrupt_ctrl(&InterruptController) != XST_SUCCESS) return XST_SUCCESS;

	// Setup interrupts for UARTS and system timer
	if(interrupt_ctrl_setup(&InterruptController, XPAR_MICROBLAZE_0_INTC_AXI_TIMER_0_INTERRUPT_INTR, XTmrCtr_InterruptHandler, (void *) &TimerSys) != XST_SUCCESS) return XST_SUCCESS;
	if(interrupt_ctrl_setup(&InterruptController, XPAR_MICROBLAZE_0_INTC_USB_UART_INTERRUPT_INTR, InterruptHandler_UART, (void *) &UartBuffDebug) != XST_SUCCESS) return XST_SUCCESS;
	if(interrupt_ctrl_setup(&InterruptController, XPAR_MICROBLAZE_0_INTC_AXI_UARTLITE_3PI_INTERRUPT_INTR, InterruptHandler_UART, (void *) &UartBuffRobot) != XST_SUCCESS) return XST_SUCCESS;

	// Set ultrasound mode
	usarray_set_mode(US_M_COMPLETE);

	// Test us_receiver FSL bus
	//TestFSL();

	// Startup message - printed via UART routines now UART initialised
	uart_print(&UartBuffDebug, "#Ready!\n");

	// Main loop
	while(1) {
		// Execute tasks
		ProcessSerialDebug();
		ProcessSerial3PI();
		ProcessUSArray();
		heartBeat();
	}

	// Just in case we made a mess although we'll never get here :-(
	cleanup_platform();

	// Yey!
	return XST_SUCCESS;
}

// --------------------------------------------------------------------------------

void ProcessSerialDebug() {
	// Attempt to read byte from debug UART if there is no command pending
	if(debugCommand == DEBUG_CMD_NONE) debugCommand = uart_getchar(&UartBuffDebug);

	// Attempt to process command
	switch(debugCommand) {
		case DEBUG_CMD_NONE: {
			// Do nothing
			return;
		}
		case DEBUG_CMD_SET_DEBUG: {
			// Wait for data
			if(get_rx_count(&UartBuffDebug) < 1) return;

			// Read byte
			char data = uart_getchar(&UartBuffDebug);

			// Check whether debug mode is being enabled or disabled
			if(data) {
				// Set debug
				debugEnabled = 0x01;

				// Output debug info
				debugPrint("DEBUG ENABLED", 1);
			} else {
				// Output debug info
				debugPrint("DEBUG DISABLED", 1);

				// Clear debug
				debugEnabled = 0x00;
			}

			break;
		}
		case DEBUG_CMD_SET_US_MODE: {

			// Wait for data
			if(get_rx_count(&UartBuffDebug) < 1) return;

			// Read byte
			char data = uart_getchar(&UartBuffDebug);

			// Set ultrasound array mode
			switch(data) {
				case 0x00: {
					// Disable array
					usarrayEnabled = 0x00;

					// Output debug info
					debugPrint("US MODE - DISABLED", 1);

					break;
				}
				case 0x01: {
					// Enable single sensor mode
					usarray_set_mode(US_M_SINGLE);

					// Enable array
					usarrayEnabled = 0x01;

					// Output debug info
					debugPrint("US MODE - SINGLE", 1);

					break;
				}
				case 0x02: {
					// Enable complete array mode
					usarray_set_mode(US_M_COMPLETE);

					// Enable array
					usarrayEnabled = 0x01;

					// Output debug info
					debugPrint("US MODE - COMPLETE", 1);

					break;
				}
				default: {
					// Output debug info
					debugPrint("US MODE - NOT RECOGNISED!", 1);
				}
			}

			break;
		}
		case DEBUG_CMD_SET_US_SENSOR: {
			// Wait for data
			if(get_rx_count(&UartBuffDebug) < 1) return;

			// Read byte
			char data = uart_getchar(&UartBuffDebug);

			// Check data
			if(data >= 0 && data < US_SENSOR_COUNT) {
				// Select sensor
				usarray_set_sensor(data);

				// Output debug info
				if(debugEnabled) {
					debugPrint("US SENSOR SELECT - ", 0);
					while(uart_putchar(&UartBuffDebug, '0' + data) == -1);
					while(uart_putchar(&UartBuffDebug, '\n') == -1);
				}
			} else {
				// Output debug info
				debugPrint("US SENSOR SELECT - NOT RECOGNISED!", 1);
			}

			break;
		}
		case DEBUG_CMD_SET_US_TRIGGERS: {
			// Wait for data
			if(get_rx_count(&UartBuffDebug) < 10) return;

			// Get data, being a bit naughty
			unsigned short data[5];
			int i;
			unsigned char* dataPtr = (unsigned char*) &data;
			for(i = 0; i < 10; i++) *dataPtr++ = uart_getchar(&UartBuffDebug);

			// Execute command
			usarray_set_triggers(data[0], data[1], data[2], data[3], data[4]);

			// Output debug info
			if(debugEnabled) {
				debugPrint("US TRIGGER SET - CHANGEOVER: ", 0);
				uart_print_int(&UartBuffDebug, data[0], 0);
				uart_print(&UartBuffDebug, ", NL: ");
				uart_print_int(&UartBuffDebug, data[1], 0);
				uart_print(&UartBuffDebug, ", NU: ");
				uart_print_int(&UartBuffDebug, data[2], 0);
				uart_print(&UartBuffDebug, ", FL: ");
				uart_print_int(&UartBuffDebug, data[3], 0);
				uart_print(&UartBuffDebug, ", FU: ");
				uart_print_int(&UartBuffDebug, data[4], 0);
				while(uart_putchar(&UartBuffDebug, '\n') == -1);
			}

			break;
		}
		case DEBUG_CMD_SET_US_OUTPUT: {
			// Wait for data
			if(get_rx_count(&UartBuffDebug) < 1) return;

			// Read byte
			char data = uart_getchar(&UartBuffDebug);

			// Set ultrasound output mode
			switch(data) {
				case 0x00: {
					// Disable output
					usarrayOutputMode = US_OUTPUT_NONE;

					// Output debug info
					debugPrint("US OUTPUT - DISABLED", 1);

					break;
				}
				case 0x01: {
					// Enable waveform output
					usarrayOutputMode = US_OUTPUT_WAVEFORM;

					// Output debug info
					debugPrint("US OUTPUT - WAVEFORM", 1);

					break;
				}
				case 0x02: {
					// Enable range output
					usarrayOutputMode = US_OUTPUT_RANGE;

					// Output debug info
					debugPrint("US OUTPUT - RANGE", 1);

					break;
				}
				default: {
					// Output debug info
					debugPrint("US OUTPUT - NOT RECOGNISED!", 1);
				}
			}

			break;
		}
		case DEBUG_CMD_ROBOT_COMMAND: {
			// Messy command unfortunately, the number of bytes to transfer to the robot must be sent
			if(debugRobotCmdSizeReceived == 0) {
				// Wait for data length
				if(get_rx_count(&UartBuffDebug) < 1) return;

				// Read length
				debugRobotCmdSizeReceived = uart_getchar(&UartBuffDebug);
			}

			// Wait for data
			if(get_rx_count(&UartBuffDebug) < debugRobotCmdSizeReceived) return;

			// Transfer command directly to robot
			while(debugRobotCmdSizeReceived > 0) {
				// Get data
				char data = uart_getchar(&UartBuffDebug);

				// Put data
				while(uart_putchar(&UartBuffRobot, data) == -1);

				// Decrement data count
				debugRobotCmdSizeReceived--;
			}

			// Output debug info
			debugPrint("ROBOT CMD ISSUED", 1);

			break;
		}
		case DEBUG_CMD_PING: {
			// Output debug info
			debugPrint("PING!", 1);

			break;
		}
		case DEBUG_CMD_ROBOT_PASSTHROUGH: {
			// Output debug info
			debugPrint("ENTERING 3PI PASSTHROUGH MODE...", 1);

			// No going back once we've done this
			Passthrough3PI();

			break;
		}
		default: {
			// Output debug info
			debugPrint("ERROR CMD NOT RECOGNISED!", 1);
		}
	}

	// Reset command
	debugCommand = DEBUG_CMD_NONE;
}

void ProcessSerial3PI() {
	// Attempt to read byte from robot UART if there is no response pending
	if(mpResponse == PLATFORM_RESP_NONE) mpResponse = uart_getchar(&UartBuffRobot);

	// Strip debugging messages from platform
	while(mpResponse != PLATFORM_RESP_NONE) {
		// Special case detect and forward debug messages from the platform to PC
		if(!mpDebugParse && mpResponse == '#') {
			// Enter debug parsing mode
			mpDebugParse = 0x01;

			// Reset debug buffer index
			mpDebugIndex = 0;
		} else if(mpDebugParse && mpResponse == '\n') {
			// Exit debug parsing mode
			mpDebugParse = 0x00;

			// Null terminate message
			mpDebugBuf[mpDebugIndex] = '\0';

			// Print message
			if(debugEnabled) {
				debugPrint("3PI: ", 0);
				uart_print(&UartBuffDebug, (char*) &mpDebugBuf);
				while(uart_putchar(&UartBuffDebug, '\n') == -1);
			}

			// Attempt to read another byte from UART
			mpResponse = uart_getchar(&UartBuffRobot);
		}

		// We may have entered parsing mode
		if(mpDebugParse) {
			// Check buffer has space (taking into account null terminator)
			if(mpDebugIndex < (MP_DEBUG_BUF_SIZE - 1)) {
				// Store character
				mpDebugBuf[mpDebugIndex++] = mpResponse;
			}

			// Attempt to read another byte from UART
			mpResponse = uart_getchar(&UartBuffRobot);
		} else {
			// Haven't entered parsing mode, or we've just exited lets try processing the command
			break;
		}
	}

	// Attempt to process response
	switch(mpResponse) {
		case PLATFORM_RESP_NONE: {
			// Do nothing
			return;
		}
		case PLATFORM_RESP_OK: {
			// Command succeeded - Output debug info
			debugPrint("3PI CMD OK", 1);
			break;
		}
		case PLATFORM_RESP_ERR: {
			// Command failed - Output debug info
			debugPrint("3PI CMD ERROR", 1);
			break;
		}
		case PLATFORM_RESP_POS: {
			// Position update received - wait for data
			if(get_rx_count(&UartBuffRobot) < 6) return;

			// Get data
			unsigned short data[3];
			int i;
			unsigned char* dataPtr = (unsigned char*) &data;
			for(i = 0; i < 6; i++) *dataPtr++ = uart_getchar(&UartBuffRobot);

			// Update position
			mpCurrentPos.X = (short) data[0];
			mpCurrentPos.Y = (short) data[1];
			mpCurrentPos.Theta = (short) data[2];

			// Output debug info
			if(debugEnabled) {
				debugPrint("3PI POS UPDATE: ", 0);
				uart_print_int(&UartBuffDebug, mpCurrentPos.X, 1);
				while(uart_putchar(&UartBuffDebug, ',') == -1);
				uart_print_int(&UartBuffDebug, mpCurrentPos.Y, 1);
				while(uart_putchar(&UartBuffDebug, ',') == -1);
				uart_print_int(&UartBuffDebug, mpCurrentPos.Theta, 0);
				while(uart_putchar(&UartBuffDebug, '\n') == -1);
			}
			break;
		}
		case PLATFORM_RESP_BTN: {
			// Button press received - wait for data
			if(get_rx_count(&UartBuffRobot) < 1) return;

			// Read byte
			char data = uart_getchar(&UartBuffRobot);

			// Output debug info
			if(debugEnabled) {
				debugPrint("3PI BTN PRESS: ", 0);
				while(uart_putchar(&UartBuffDebug, '0' + data) == -1);
				while(uart_putchar(&UartBuffDebug, '\n') == -1);
			}
			break;
		}
	}

	// Reset response
	mpResponse = PLATFORM_RESP_NONE;
}

void ProcessUSArray() {
	// Retrieve current state
	switch(usarray_get_status()) {
		case US_S_ADC_ERROR_REQUEST:
		case US_S_ADC_ERROR_RESPONSE:
		{
			// Output debug info - only when array is enabled
			if(usarrayEnabled) {
				debugPrint("US ARRAY ADC ERROR!!!", 1);
			}
		}
		case US_S_IDLE:
		{
			// Start next scan if array is enabled
			if(usarrayEnabled) {
				// Check if temperature reading taken
				if(usarrayTempTaken) {
					// Select first sensor (for continuous mode)
					if(usarray_get_mode() == US_M_COMPLETE) {
						usarray_set_sensor(0);
					}

					// Start first ranging operation
					//print("##Scanning\n");
					usarray_scan();
				} else {
					//print("##Temperature\n");
					// Take temperature reading
					usarray_measure_temp();

					// Set flag
					usarrayTempTaken = 0x01;
				}
			}

			break;
		}
		case US_S_COMPLETE:
		{
			//print("##Update ranges\n");
			// Update range array
			usarray_update_ranges();

			// Output data if requested
			if(usarrayOutputMode != US_OUTPUT_NONE) {
				// Compute sensor start and end index
				int startIndex = (usarray_get_mode() == US_M_SINGLE ? usarray_get_sensor() : 0);
				int endIndex = (usarray_get_mode() == US_M_SINGLE ? usarray_get_sensor() + 1 : US_SENSOR_COUNT);

				// Output start message
				//uart_print(&UartBuffDebug, "START\n");
				uart_print_char(&UartBuffDebug, 0xFF);
				uart_print_char(&UartBuffDebug, 0xFF);

				// Output data
				int i, j;
				switch(usarrayOutputMode) {
					case US_OUTPUT_NONE: {
						// Do nothing
						break;
					}
					case US_OUTPUT_WAVEFORM: {
						// Output data for each required sensor
						for(i = startIndex; i < endIndex; i++) {
							for(j = 0; j < US_RX_COUNT; j++) {
								// Print waveform data
								//uart_print_int(&UartBuffDebug, i, 0);
								//while(uart_putchar(&UartBuffDebug, ',') == -1);
								//uart_print_int(&UartBuffDebug, usWaveformData[i][j], 0);
								//while(uart_putchar(&UartBuffDebug, '\n') == -1);

								char first = ((usWaveformData[i][j] & 0xF00) >> 4) | (i & 0x0F);
								char second = usWaveformData[i][j] & 0xFF;
								uart_print_char(&UartBuffDebug, first);
								uart_print_char(&UartBuffDebug, second);
							}
						}
						break;
					}
					case US_OUTPUT_RANGE: {
						// Output data for each required sensor
						for(i = startIndex; i < endIndex; i++) {
							// Print range data
							//uart_print_int(&UartBuffDebug, i, 0);
							//while(uart_putchar(&UartBuffDebug, ',') == -1);
							//uart_print_int(&UartBuffDebug, usRangeReadings[i], 1);
							//while(uart_putchar(&UartBuffDebug, '\n') == -1);

							char first = ((usRangeReadings[i] & 0xF00) >> 4) | (i & 0x0F);
							char second = usRangeReadings[i] & 0xFF;
							uart_print_char(&UartBuffDebug, first);
							uart_print_char(&UartBuffDebug, second);
						}
						break;
					}
				}

				// Output end message
				//uart_print(&UartBuffDebug, "END\n");
				uart_print_char(&UartBuffDebug, 0x7F);
				uart_print_char(&UartBuffDebug, 0xFF);
			}

			// Get ready for next reading
			usarray_reset();

			break;
		}
		case US_S_ADC_REQUEST:
		case US_S_ADC_RESPONSE:
		case US_S_TEMP:
			// Do nothing - ranging or temperature measurement in progress
			break;
	}
}

void Passthrough3PI() {
	char c;

	// Forever
	while(1) {
		// Read character from PC and pass to 3pi
		c = uart_getchar(&UartBuffDebug);
		if(c != -1) while(uart_putchar(&UartBuffRobot, c) == -1);

		// Read character from 3pi and pass to PC
		c = uart_getchar(&UartBuffRobot);
		if(c != -1) while(uart_putchar(&UartBuffDebug, c) == -1);

		// Show we're still alive
		heartBeat();
	}
}

// --------------------------------------------------------------------------------

void TestFSL() {

	print("\n##Starting FSL Test...\n");

	u32 output = 0xABCDEF0;
	u8 status;
	u8 type;
	u32 data;

	sendUSEcho(output);
	xil_printf("#Sent:   0x%07x\n", (unsigned int)output);

	readUSData(&status, &type, &data);
	xil_printf("#Read:   0x%07x\n", (unsigned int)data);
	xil_printf("#Status: 0x%01x\n", (unsigned int)status);
	xil_printf("#Type:   0x%01x\n", (unsigned int)type);

	if (status == US_STATUS_OK && type == US_RESP_ECHO && data == output)
		print("##FSL Test Passed\n");
	else
		print("##FSL Test Failed\n");
}

// --------------------------------------------------------------------------------

void heartBeat() {
	static unsigned int heartbeatTime = 0;
	static unsigned char heartbeatState = 0;

	// Toggle hear beat every x ms
	if(sysTickCounter > heartbeatTime) {
		heartbeatState = ~heartbeatState;
		gpio_write_bit(&gpioLEDS, 0, heartbeatState);
		heartbeatTime += HEARTBEAT_INTERVAL;
	}
}

// --------------------------------------------------------------------------------

void mpSetDebug(unsigned char state) {
	// Send debug enable command
	while(uart_putchar(&UartBuffRobot, PLATFORM_CMD_SET_DEBUG) == -1);
	while(uart_putchar(&UartBuffRobot, state) == -1);
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

// --------------------------------------------------------------------------------

void InterruptHandler_Timer_Sys(void *CallbackRef) {
	// Increment system tick counter
	sysTickCounter++;
}

// --------------------------------------------------------------------------------

void debugPrint(char* str, char newLine) {
	// Print information to UART if debugging enabled
	if(debugEnabled) {
		uart_print(&UartBuffDebug, "#DBG: ");
		uart_print(&UartBuffDebug, str);
		if(newLine) while(uart_putchar(&UartBuffDebug, '\n') == -1);
	}
}

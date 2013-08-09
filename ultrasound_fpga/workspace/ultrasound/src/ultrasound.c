#include "ultrasound.h"

// --------------------------------------------------------------------------------

// Variables - devices
XIntc InterruptController; // Interrupt controller shared between all devices
uart_buff UartBuffDebug; // UART connection between PC and FPGA
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
u8 sensors[10]; // Array of sensors to sample
u8 numSensors = 0; // Number of sensors to sample (size of sensor array)

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

	// Set active ultrasound sensors
	numSensors = 6;
	sensors[0] = SENSOR_FRONT_RIGHT;
	sensors[1] = SENSOR_LEFT_MID;
	sensors[2] = SENSOR_RIGHT_FRONT;
	sensors[3] = SENSOR_LEFT_FRONT;
	sensors[4] = SENSOR_RIGHT_MID;
	sensors[5] = SENSOR_FRONT_LEFT;

	// Test us_receiver FSL bus
	//TestFSL();

	// Initialise 3PI robot
	Init3PI();

	// Get temperature from ADC
	usarray_measure_temp();

	// Startup message - printed via UART routines now UART initialised
	uart_print(&UartBuffDebug, "Ready!\n");

	// Main loop
	while(1) {
		// Execute tasks
		ProcessSerialDebug();
		ProcessSerial3PI();
		ProcessUSArray();
		Drive3PI();
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
					numSensors = 1;
					sensors[0] = usarray_get_sensor();

					// Enable array
					usarrayEnabled = 0x01;

					// Output debug info
					debugPrint("US MODE - SINGLE", 1);

					break;
				}
				case 0x02: {
					// Enable complete array mode
					numSensors = 10;
					sensors[0] = 0;
					sensors[1] = 1;
					sensors[2] = 2;
					sensors[3] = 3;
					sensors[4] = 4;
					sensors[5] = 5;
					sensors[6] = 6;
					sensors[7] = 7;
					sensors[8] = 8;
					sensors[9] = 9;

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
				if (numSensors == 1)
					sensors[0] = data;

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
	// Start next scan if array is enabled
	if(usarrayEnabled && numSensors > 0) {
		// Start first ranging operation
		usarray_scan(sensors, numSensors);

		// Update range array
		usarray_update_ranges(sensors, numSensors);

		// Output data if requested
		if(usarrayOutputMode != US_OUTPUT_NONE) {

			// Output start message
			uart_print_char(&UartBuffDebug, 0xFF);
			uart_print_char(&UartBuffDebug, 0xFF);

			// Output data
			int i, j;
			u8 sensorNum;
			switch(usarrayOutputMode) {
				case US_OUTPUT_NONE: {
					// Do nothing
					break;
				}
				case US_OUTPUT_WAVEFORM: {
					// Output data for each required sensor
					for(i = 0; i < numSensors; i++) {
						for(j = 0; j < US_RX_COUNT; j++) {
							sensorNum = sensors[i];
							// Print waveform data
							char first = ((usWaveformData[sensorNum][j] & 0xF00) >> 4) | (sensorNum & 0x0F);
							char second = usWaveformData[sensorNum][j] & 0xFF;
							uart_print_char(&UartBuffDebug, first);
							uart_print_char(&UartBuffDebug, second);
						}
					}
					break;
				}
				case US_OUTPUT_RANGE: {
					// Output data for each required sensor
					for(i = 0; i < numSensors; i++) {
						sensorNum = sensors[i];
						// Print range data
						char first = ((usRangeReadings[sensorNum] & 0xF00) >> 4) | (i & 0x0F);
						char second = usRangeReadings[sensorNum] & 0xFF;
						uart_print_char(&UartBuffDebug, first);
						uart_print_char(&UartBuffDebug, second);
					}
					break;
				}
			}

			// Output end message
			uart_print_char(&UartBuffDebug, 0x7F);
			uart_print_char(&UartBuffDebug, 0xFF);
		}
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

void Init3PI() {

	mpBeep();

	// Set manual mode
	mpSetMode(0x00);
}

void Drive3PI() {

	u8 front = usarray_detect_obstacle(7, 60) || usarray_detect_obstacle(8, 60);
	u8 frontLeft = usarray_detect_obstacle(6, 40);
	u8 frontRight = usarray_detect_obstacle(9, 40);
	u8 left = usarray_detect_obstacle(5, 40);
	u8 right = usarray_detect_obstacle(0, 40);
	u8 farLeft = usarray_detect_obstacle(5, 80);

	// Change direction based on sensor readings
	if (frontLeft) {
		mpSetMotorSpeed(PLATFORM_DIR_RIGHT, 30, 30);
	}
	else if (frontRight) {
		mpSetMotorSpeed(PLATFORM_DIR_LEFT, 30, 30);
	}
	else if (front && !farLeft) {
		mpSetMotorSpeed(PLATFORM_DIR_LEFT, 30, 30);
	}
	else if (front) {
		mpSetMotorSpeed(PLATFORM_DIR_RIGHT, 30, 30);
	}
	else if (left) {
		mpSetMotorSpeed(PLATFORM_DIR_FORWARD, 30, 10);
	}
	else if (right) {
		mpSetMotorSpeed(PLATFORM_DIR_FORWARD, 10, 30);
	}
	else {
		mpSetMotorSpeed(PLATFORM_DIR_FORWARD, 30, 30);
	}
}

// --------------------------------------------------------------------------------

void heartBeat() {
	static unsigned int heartbeatTime = 0;
	static unsigned char heartbeatState = 0;

	// Toggle heart beat every x ms
	if(sysTickCounter > heartbeatTime) {
		heartbeatState = ~heartbeatState;
		gpio_write_bit(&gpioLEDS, 0, heartbeatState);
		heartbeatTime += HEARTBEAT_INTERVAL;
	}
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

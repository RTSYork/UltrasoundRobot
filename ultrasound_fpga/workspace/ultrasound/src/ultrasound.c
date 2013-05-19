#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xparameters.h"

#include "int_ctrl.h"
#include "uart.h"
#include "gpio.h"
#include "timer.h"
#include "usarray.h"

// --------------------------------------------------------------------------------

// Function prototypes

int ProcessSerialDebug();
int ProcessSerial3PI();
int ProcessUSArray();
int Passthrough3PI();

void InterruptHandler_Timer_Sys(void *CallbackRef); // Increment system tick counter

// --------------------------------------------------------------------------------

// Variables - general
volatile u32 sysTickCounter = 0;
unsigned char debugMode = 0; // Print debugging information to development PC
unsigned char pcWaveformOutput = 0; // Output waveform data to development PC

// Variables - devices
XIntc InterruptController; // Interrupt controller shared between all devices
volatile uart_buff UartBuffDebug; // UART connection between PC and FPGA
volatile uart_buff UartBuffRobot; // UART connection between FPGA and 3PI
gpio_state gpioDipSwitches; // GPIO for 4 on-board dip switches
gpio_state gpioLEDS; // GPIO for 4 on-board LEDS
gpio_state gpioUSDebug; // GPIO for ultrasound ADC end of conversion interrupt & debug IO
XTmrCtr TimerSys; // Timer for delays

// --------------------------------------------------------------------------------

int main() {
	// Get ready to rumble
	init_platform();

	// Startup message - printed directly to UART
	print("Ultrasound....");

	// Init UARTS
	if(init_uart_buffers(XPAR_USB_UART_DEVICE_ID, &UartBuffDebug) != XST_SUCCESS) return XST_FAILURE;
	if(init_uart_buffers(XPAR_AXI_UARTLITE_3PI_DEVICE_ID, &UartBuffRobot) != XST_SUCCESS) return XST_FAILURE;

	// Init GPIO
	if(init_gpio(XPAR_DIP_SWITCHES_4BITS_DEVICE_ID, 0x0F, 0x00, &gpioDipSwitches) != XST_SUCCESS) return XST_FAILURE;
	if(init_gpio(XPAR_LEDS_4BITS_DEVICE_ID, 0x00, 0x00, &gpioLEDS) != XST_SUCCESS) return XST_FAILURE;
	if(init_gpio(XPAR_AXI_GPIO_US_DEVICE_ID, 0x01, 0x01, &gpioUSDebug) != XST_SUCCESS) return XST_FAILURE;

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

	// Setup interrupts for ultrasound array
	if(interrupt_ctrl_setup(&InterruptController, XPAR_MICROBLAZE_0_INTC_AXI_TIMER_US_INTERRUPT_INTR, XTmrCtr_InterruptHandler, (void *) &TimerUs) != XST_SUCCESS) return XST_SUCCESS;
	if(interrupt_ctrl_setup(&InterruptController, XPAR_MICROBLAZE_0_INTC_AXI_GPIO_US_IP2INTC_IRPT_INTR, InterruptHandler_US_GPIO, (void *) &gpioUSDebug) != XST_SUCCESS) return XST_SUCCESS;

	// Set ultrasound mode
	usarray_setmode(US_M_COMPLETE);

	// Startup message - printed via UART routines now UART initialised
	uart_print(&UartBuffDebug, "Ready!\n\r");

	// Main loop
	unsigned int heartbeatTime = 0;
	unsigned char heartbeatState = 0;
	while(1) {
		// Execute tasks
		ProcessSerialDebug();
		ProcessSerial3PI();
		ProcessUSArray();

		//Passthrough3PI();

		// Toggle hear beat every 500ms
		if(sysTickCounter > heartbeatTime) {
			heartbeatState = ~heartbeatState;
			gpio_write_bit(&gpioLEDS, 0, heartbeatState);
			heartbeatTime += 1000;
		}
	}

	// Just in case we made a mess although we'll never get here :-(
	cleanup_platform();

	// Yey!
	return XST_SUCCESS;
}

// --------------------------------------------------------------------------------

int ProcessSerialDebug() {
	// Yey!
	return XST_SUCCESS;
}

int ProcessSerial3PI() {
	// Yey!
	return XST_SUCCESS;
}

int ProcessUSArray() {
	// Retrieve current state
	switch(usarray_get_status()) {
		case US_S_ADC_ERROR_REQUEST:
		case US_S_ADC_ERROR_RESPONSE:
		{
			// Output in debug mode and fall through to start next scan
		}
		case US_S_IDLE:
		{
			// Select first sensor (for continuous mode)
			if(usarray_get_mode() == US_M_COMPLETE) {
				usarray_selectSensor(0);
			}

			// Start first ranging operation
			usarray_scan();

			break;
		}
		case US_S_COMPLETE:
		{
			// Update range array
			usarray_update_ranges();

			// Output data if requested
			/*
			print("START\n");
			int i;
			for(i = 0; i < US_RX_COUNT; i++) printf("%i\n", usWaveformData[8][i]);
			print("END\n");
			*/

			// Get ready for next reading
			usarray_reset();

			break;
		}
		case US_S_ADC_REQUEST:
		case US_S_ADC_RESPONSE:
			// Do nothing - ranging in progress
			break;
	}

	// Yey!
	return XST_SUCCESS;
}

int Passthrough3PI() {
	int c;

	// Read character from PC and pass to 3pi
	c = uart_getchar(&UartBuffDebug);
	if(c != -1) while(uart_putchar(&UartBuffRobot, c) == -1);

	// Read character from 3pi and pass to PC
	c = uart_getchar(&UartBuffRobot);
	if(c != -1) while(uart_putchar(&UartBuffDebug, c) == -1);

	// Yey!
	return XST_SUCCESS;
}

// --------------------------------------------------------------------------------

void InterruptHandler_Timer_Sys(void *CallbackRef) {
	// Increment system tick counter
	sysTickCounter++;
}

#include "gpio.h"

int init_gpio(int deviceID, u32 direction_mask, u32 interrupt_mask, gpio_state* gpioState) {
	// Init GPIO
	if(XGpio_Initialize(&(gpioState->gpio), deviceID) != XST_SUCCESS) return XST_FAILURE;

	// Set pin directions
	XGpio_SetDataDirection(&(gpioState->gpio), 1, direction_mask);

	// Set interrupts
	if(interrupt_mask) {
		// Set interrupt mask
		XGpio_InterruptEnable(&(gpioState->gpio), interrupt_mask);

		// Enable interrupts
		XGpio_InterruptGlobalEnable(&(gpioState->gpio));
	}

	// Yey!
	return XST_SUCCESS;
}

u32 gpio_read(gpio_state* gpioState) {
	// Read state
	return XGpio_DiscreteRead(&(gpioState->gpio), 0);
}

unsigned char gpio_read_bit(gpio_state* gpioState, unsigned char bit) {
	// Calculate mask
	u32 mask = (((u32) 1) << bit);

	// Read state and extract bit
	return (XGpio_DiscreteRead(&(gpioState->gpio), 0) & mask);
}

void gpio_write(gpio_state* gpioState, u32 value) {
	// Update state
	gpioState->state = value;

	// Write state
	XGpio_DiscreteWrite(&(gpioState->gpio), 1, gpioState->state);
}

void gpio_write_bit(gpio_state* gpioState, unsigned char bit, unsigned char value) {
	// Calculate mask
	u32 mask = (((u32) 1) << bit);

	// Update state
	if(value) {
		gpioState->state |= mask;
	} else {
		gpioState->state &= ~mask;
	}

	// Write state
	XGpio_DiscreteWrite(&(gpioState->gpio), 1, gpioState->state);
}

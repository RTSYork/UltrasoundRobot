#ifndef GPIO_H_
#define GPIO_H_

#include "xgpio.h"

typedef struct gpio_state {
	XGpio gpio;
	u32 state;
} gpio_state;

int init_gpio(int deviceID, u32 direction_mask, u32 interrupt_mask, gpio_state* gpioState);

u32 gpio_read(gpio_state* gpioState);
void gpio_write(gpio_state* gpioState, u32 value);
void gpio_write_bit(gpio_state* gpioState, unsigned char bit, unsigned char value);
unsigned char gpio_read_bit(gpio_state* gpioState, unsigned char bit);

#endif /* GPIO_H_ */

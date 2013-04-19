#include "led.h"

#include "lpc17xx_gpio.h"

//Pin masks for mbed onboard LEDs
const uint32_t led_mask[4] = {1 << 18, 1 << 20, 1 << 21, 1 << 23};

//LED states
uint32_t led_state = 0;

void led_init() {
	//Pin mask for all mbed onboard LEDs
	uint32_t led_mask_all = led_mask[0] | led_mask[1] | led_mask[2] | led_mask[3];

	//Set the direction of mbed onboard LED pins to output.
	GPIO_SetDir(1, led_mask_all, 1);

	//Turn off all leds
	GPIO_ClearValue(1, led_mask_all);
}

void led_write(uint8_t num, uint8_t val) {
	//Set pin value and store led value
	if(val) {
		GPIO_SetValue(1, led_mask[num]);

		led_state |= led_mask[num];
	} else {
		GPIO_ClearValue(1, led_mask[num]);

		led_state &= ~led_mask[num];
	}
}

uint32_t led_read(uint8_t num) {
	//Return led value
	return led_state & led_mask[num];
}

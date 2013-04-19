#ifndef LED_H
#define LED_H

#include "lpc_types.h"

void led_init();

void led_write(uint8_t num, uint8_t val);
uint32_t led_read(uint8_t num);

#endif

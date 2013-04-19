#ifndef TIMER_H
#define TIMER_H

#include "lpc17xx_timer.h"

void timer_init();

void timer_start(uint8_t num);
void timer_stop(uint8_t num);

#endif

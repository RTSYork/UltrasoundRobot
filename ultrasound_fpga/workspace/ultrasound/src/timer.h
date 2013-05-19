#ifndef TIMER_H_
#define TIMER_H_

#include "xtmrctr.h"

int init_timer(int deviceID, XTmrCtr *timer, u32 timer_freq, u32 target_freq, XTmrCtr_Handler interruptHandler);

void timer_setstate(XTmrCtr *timer, char state);

#endif /* TIMER_H_ */

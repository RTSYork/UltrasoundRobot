#ifndef INT_CTRL_H_
#define INT_CTRL_H_

#include "xintc.h"
#include "xil_exception.h"

int init_interrupt_ctrl(XIntc *int_ctrl); // Initialise interrupt controller

int interrupt_ctrl_setup(XIntc *int_ctrl, u8 interruptID, XInterruptHandler interruptHandler, void *interruptCallbackRef); // Configure interrupt handler
int interrupt_fast_ctrl_setup(XIntc *int_ctrl, u8 interruptID, XFastInterruptHandler interruptHandler);

#endif /* INT_CTRL_H_ */

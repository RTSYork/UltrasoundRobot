#include "timer.h"

int init_timer(int deviceID, XTmrCtr *timer, u32 timer_freq, u32 target_freq, XTmrCtr_Handler interruptHandler) {
	// Init timer
	if(XTmrCtr_Initialize(timer, deviceID) != XST_SUCCESS) return XST_FAILURE;

	// Self test
	if(XTmrCtr_SelfTest(timer, 0) != XST_SUCCESS) return XST_FAILURE;

	// Setup handler to be called when timer expires
	XTmrCtr_SetHandler(timer, interruptHandler, (void*) timer);

	// Enable interrupts and auto-reset so timer runs forever
	XTmrCtr_SetOptions(timer, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);

	// Set reset value to cause timer to roll-over before it reaches zero at the specified frequency
	XTmrCtr_SetResetValue(timer, 0, ((unsigned int) 0xFFFFFFFF) - (timer_freq / target_freq));

	// Yey!
	return XST_SUCCESS;
}

void timer_setstate(XTmrCtr *timer, char state) {
	// Enable / disable timer
	if(state) {
		XTmrCtr_Start(timer, 0);
	} else {
		XTmrCtr_Stop(timer, 0);
	}
}

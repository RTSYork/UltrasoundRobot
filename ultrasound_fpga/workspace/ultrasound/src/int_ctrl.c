#include "int_ctrl.h"

int init_interrupt_ctrl(XIntc *int_ctrl) {
	// Init interrupt controller
	if(XIntc_Initialize(int_ctrl, XPAR_INTC_0_DEVICE_ID) != XST_SUCCESS) return XST_FAILURE;

	// Self test
	if(XIntc_SelfTest(int_ctrl) != XST_SUCCESS) return XST_FAILURE;

	// Start interrupt controller, allowing it to interrupt the microblaze
	if(XIntc_Start(int_ctrl, XIN_REAL_MODE) != XST_SUCCESS) return XST_FAILURE;

	// Init exceptions table
	Xil_ExceptionInit();

	// Register interrupt controller with exceptions table
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XIntc_InterruptHandler, int_ctrl);

	// Enable exceptions
	Xil_ExceptionEnable();

	// Yey!
	return XST_SUCCESS;
}

int interrupt_ctrl_setup(XIntc *int_ctrl, u8 interruptID, XInterruptHandler interruptHandler, void *interruptCallbackRef) {
	// Connect interrupt handler
	if(XIntc_Connect(int_ctrl, interruptID, interruptHandler, interruptCallbackRef) != XST_SUCCESS) return XST_FAILURE;

	// Enable interrupt
	XIntc_Enable(int_ctrl, interruptID);

	// Yey!
	return XST_SUCCESS;
}

int interrupt_fast_ctrl_setup(XIntc *int_ctrl, u8 interruptID, XFastInterruptHandler interruptHandler) {
	// Connect interrupt handler
	if(XIntc_ConnectFastHandler(int_ctrl, interruptID, interruptHandler) != XST_SUCCESS) return XST_FAILURE;

	// Enable interrupt
	XIntc_Enable(int_ctrl, interruptID);

	// Yey!
	return XST_SUCCESS;
}

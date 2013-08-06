/*****************************************************************************
* us_receiver (FSL bus) Driver Source File
*****************************************************************************/

#include "us_receiver.h"

/**
 * Send an echo
 */
void sendUSEcho(u32 data) {
	sendUSCommand(US_COMM_ECHO, data);
}

/**
 * Send an reset command
 */
void sendUSReset(void) {
	sendUSCommand(US_COMM_RESET, 0);
}

/**
 * Send an init command
 */
void sendUSInit(void) {
	sendUSCommand(US_COMM_INIT, 0);
}

/**
 * Send a temperature request
 */
void sendUSTempRequest(void) {
	sendUSCommand(US_COMM_TEMP, 0);
}

/**
 * Send a sample request
 */
void sendUSSampleRequest(u8 sensor, u16 count, u16 period) {
	u32 data = ((period & 0xFFF) << 14) | ((count & 0x1FF) << 4) | (sensor & 0xF);
	sendUSCommand(US_COMM_SAMPLE, data);
}


/**
 * Send command and data over FSL bus
 */
void sendUSCommand(u8 command, u32 data) {
	u32 output = (data << 4) | (command & 0xF);
	putfsl(output, US_RECEIVER_FSL_SLOT_ID);
}

/**
 * Read result (status, type and data) from FSL bus
 */
void readUSData(u8* status, u8* type, u32* data) {
	u32 input;
	getfsl(input, US_RECEIVER_FSL_SLOT_ID);

	*status = input & 0x1;
	*type = (input >> 1) & 0x7;
	*data = (input >> 4);
}

/**
 * Test FSL bus
 */
int testUSFSL(void) {
	u32 output = 0xABCDEF0;
	u8 status;
	u8 type;
	u32 data;

	sendUSEcho(output);
	readUSData(&status, &type, &data);

	if (status == US_STATUS_OK && type == US_RESP_ECHO && data == output)
		return XST_SUCCESS;
	else
		return XST_FAILURE;
}

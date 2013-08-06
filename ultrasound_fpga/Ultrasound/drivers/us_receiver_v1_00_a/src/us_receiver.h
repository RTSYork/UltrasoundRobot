/*****************************************************************************
* us_receiver (FSL bus) Driver Header File
*****************************************************************************/

#ifndef US_RECEIVER_H
#define US_RECEIVER_H

#include "xstatus.h"
#include "fsl.h"

// Assumes FSL slot 0
#define US_RECEIVER_FSL_SLOT_ID 0

// Commands
#define US_COMM_ECHO   0x0
#define US_COMM_TEMP   0x1
#define US_COMM_SAMPLE 0x2
#define US_COMM_INIT   0x3
#define US_COMM_RESET  0xF

// Response types
#define US_RESP_ECHO    0x0
#define US_RESP_TEMP    0x1
#define US_RESP_SAMPLE  0x2
#define US_RESP_NONE    0x3
#define US_RESP_UNKNOWN 0x7

// Statuses
#define US_STATUS_OK    0x0
#define US_STATUS_ERROR 0x1

void sendUSEcho(u32 data);
void sendUSReset(void);
void sendUSInit(void);
void sendUSTempRequest(void);
void sendUSSampleRequest(u8 sensor, u16 count, u16 period);

void sendUSCommand(u8 command, u32 data);
void readUSData(u8* status, u8* type, u32* data);

int testUSFSL(void);

#endif 

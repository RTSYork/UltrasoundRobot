/*****************************************************************************
* Filename:          C:\temp\ultrasound_fpga\Ultrasound/drivers/pulsegen_v1_00_a/src/pulsegen.c
* Version:           1.00.a
* Description:       pulsegen Driver Source File
*****************************************************************************/


/***************************** Include Files *******************************/

#include "pulsegen.h"

/************************** Function Definitions ***************************/

void pulseGen_GeneratePulse(u32 baseAddress, u8 enable, u8 address, u16 cycleCount) {
	PULSEGEN_mWriteSlaveReg0(baseAddress, 0, (enable << 16) | (cycleCount << 4) | address);
}
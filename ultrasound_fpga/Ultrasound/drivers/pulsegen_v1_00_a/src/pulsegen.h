/*****************************************************************************
* Filename:          C:\temp\ultrasound_fpga\Ultrasound/pulsegen_v1_00_a/src/pulsegen.h
* Version:           1.00.a
* Description:       pulsegen Driver Header File
*****************************************************************************/

#ifndef PULSEGEN_H
#define PULSEGEN_H

/***************************** Include Files *******************************/

#include "xbasic_types.h"
#include "xstatus.h"
#include "xil_io.h"

/************************** Constant Definitions ***************************/


/**
 * User Logic Slave Space Offsets
 * -- SLV_REG0 : user logic slave module register 0
 * -- SLV_REG1 : user logic slave module register 1
 */
#define PULSEGEN_USER_SLV_SPACE_OFFSET (0x00000000)
#define PULSEGEN_SLV_REG0_OFFSET  (PULSEGEN_USER_SLV_SPACE_OFFSET + 0x00000000)

/**
 * Software Reset Space Register Offsets
 * -- RST : software reset register
 */
#define PULSEGEN_SOFT_RST_SPACE_OFFSET (0x00000100)
#define PULSEGEN_RST_REG_OFFSET (PULSEGEN_SOFT_RST_SPACE_OFFSET + 0x00000000)

/**
 * Software Reset Masks
 * -- SOFT_RESET : software reset
 */
#define SOFT_RESET (0x0000000A)

/**************************** Type Definitions *****************************/



/***************** Macros (Inline Functions) Definitions *******************/

/**
 *
 * Write a value to a PULSEGEN register. A 32 bit write is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is written.
 *
 * @param   BaseAddress is the base address of the PULSEGEN device.
 * @param   RegOffset is the register offset from the base to write to.
 * @param   Data is the data written to the register.
 *
 * @return  None.
 *
 * @note
 * C-style signature:
 * 	void PULSEGEN_mWriteReg(Xuint32 BaseAddress, unsigned RegOffset, Xuint32 Data)
 *
 */
#define PULSEGEN_mWriteReg(BaseAddress, RegOffset, Data) \
 	Xil_Out32((BaseAddress) + (RegOffset), (Xuint32)(Data))

/**
 *
 * Read a value from a PULSEGEN register. A 32 bit read is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is read from the register. The most significant data
 * will be read as 0.
 *
 * @param   BaseAddress is the base address of the PULSEGEN device.
 * @param   RegOffset is the register offset from the base to write to.
 *
 * @return  Data is the data from the register.
 *
 * @note
 * C-style signature:
 * 	Xuint32 PULSEGEN_mReadReg(Xuint32 BaseAddress, unsigned RegOffset)
 *
 */
#define PULSEGEN_mReadReg(BaseAddress, RegOffset) \
 	Xil_In32((BaseAddress) + (RegOffset))


/**
 *
 * Write/Read 32 bit value to/from PULSEGEN user logic slave registers.
 *
 * @param   BaseAddress is the base address of the PULSEGEN device.
 * @param   RegOffset is the offset from the slave register to write to or read from.
 * @param   Value is the data written to the register.
 *
 * @return  Data is the data from the user logic slave register.
 *
 * @note
 * C-style signature:
 * 	void PULSEGEN_mWriteSlaveRegn(Xuint32 BaseAddress, unsigned RegOffset, Xuint32 Value)
 * 	Xuint32 PULSEGEN_mReadSlaveRegn(Xuint32 BaseAddress, unsigned RegOffset)
 *
 */
#define PULSEGEN_mWriteSlaveReg0(BaseAddress, RegOffset, Value) \
 	Xil_Out32((BaseAddress) + (PULSEGEN_SLV_REG0_OFFSET) + (RegOffset), (Xuint32)(Value))

#define PULSEGEN_mReadSlaveReg0(BaseAddress, RegOffset) \
 	Xil_In32((BaseAddress) + (PULSEGEN_SLV_REG0_OFFSET) + (RegOffset))

/**
 *
 * Reset PULSEGEN via software.
 *
 * @param   BaseAddress is the base address of the PULSEGEN device.
 *
 * @return  None.
 *
 * @note
 * C-style signature:
 * 	void PULSEGEN_mReset(Xuint32 BaseAddress)
 *
 */
#define PULSEGEN_mReset(BaseAddress) \
 	Xil_Out32((BaseAddress)+(PULSEGEN_RST_REG_OFFSET), SOFT_RESET)

	
/************************** Function Prototypes ****************************/

void pulseGen_GeneratePulse(u32 baseAddress, u8 enable, u8 address, u16 cycleCount);

/**
*  Defines the number of registers available for read and write*/
#define TEST_AXI_LITE_USER_NUM_REG 1


#endif /** PULSEGEN_H */

#include "usarray.h"

#include "xparameters.h"

#include "xspi.h"
#include "xgpio.h"
#include "pulsegen.h"

#define USVoltageToTriggerLevel(x) (unsigned short) ((((unsigned int) x) * ((1 << USADCPrecision) - 1)) / USADCReference) // Voltage expressed in hundredths
#define USSampleIndexToTime(x) (unsigned short) ((((1000000 * 10) / US_SAMPLE_RATE) * (((unsigned int) x) + 1)) / 10) // Time expressed in uS
#define USTimeToSampleIndex(x) (unsigned short) ((((unsigned int) x) * 10) / ((1000000 * 10) / US_SAMPLE_RATE) - 1) // Time in uS

const unsigned char usSensorMap[] = US_SENSOR_MAP; // Sensor position to address map

volatile enum US_MODE usMode = US_M_SINGLE; // Start in single sensor mode

volatile enum US_STATE usState = US_S_IDLE; // Start with sonar array idle
volatile unsigned char usSensorIndex = 0; // Next sensor to scan
volatile unsigned short usSampleIndex = 0; // Sample index, incremented once per ADC conversion, representative of ToF
volatile unsigned short usWaveformData[US_SENSOR_COUNT][US_RX_COUNT]; // Raw waveform data - stored as ADC results
volatile signed short usRangeReadings[US_SENSOR_COUNT]; // Latest range readings - stored as sample indexes

volatile unsigned short usTriggerChangeIndex = USTimeToSampleIndex(1200); // Trigger changeover time
volatile unsigned short usTriggerNearUpper = USVoltageToTriggerLevel(140); // Upper trigger level
volatile unsigned short usTriggerNearLower = USVoltageToTriggerLevel(164); // Lower trigger level
volatile unsigned short usTriggerFarUpper = USVoltageToTriggerLevel(146); // Upper trigger level
volatile unsigned short usTriggerFarLower = USVoltageToTriggerLevel(158); // Lower trigger level

volatile signed short usTemperature = 210; // Temperature in degrees C, expressed in tenths

XTmrCtr TimerUs; // Timer for ADC sampling
XSpi SpiADC; // SPI interface for ADC

int init_usarray() {
	// Lookup SPI config
	XSpi_Config *ConfigPtr = XSpi_LookupConfig(XPAR_AXI_SPI_US_DEVICE_ID);
	if(ConfigPtr == NULL) return XST_DEVICE_NOT_FOUND;

	// Init SPI
	if(XSpi_CfgInitialize(&SpiADC, ConfigPtr, ConfigPtr->BaseAddress) != XST_SUCCESS) return XST_FAILURE;

	// Self test SPI
	if(XSpi_SelfTest(&SpiADC) != XST_SUCCESS) return XST_FAILURE;

	// Enable master mode with automatic slave selection
	if(XSpi_SetOptions(&SpiADC, XSP_MASTER_OPTION) != XST_SUCCESS) return XST_FAILURE;

	// Select slave config - we'll always be talking to the ADC
	if(XSpi_SetSlaveSelect(&SpiADC, 0x01) != XST_SUCCESS) return XST_FAILURE;

	// Start SPI device
	XSpi_Start(&SpiADC);

	// Disable interrupts for polled mode
	XSpi_IntrGlobalDisable(&SpiADC);

	// Init timer
	if(init_timer(XPAR_AXI_TIMER_US_DEVICE_ID, &TimerUs, XPAR_AXI_TIMER_US_CLOCK_FREQ_HZ, US_SAMPLE_RATE, (XTmrCtr_Handler) &InterruptHandler_US_Timer) != XST_SUCCESS) return XST_FAILURE;

	// Setup ADC by writing to setup register
	unsigned char adcSetupByte = 0x64; // Set ADC to use its internal clock for sampling and conversions, use external single ended reference
	XSpi_Transfer(&SpiADC, (u8*) &adcSetupByte, NULL, 1);

	// Reset all ranges
	int i;
	for(i = 0; i < US_SENSOR_COUNT; i++) usRangeReadings[i] = -1;

	// Yey!
	return XST_SUCCESS;
}

void InterruptHandler_US_Timer(void *CallbackRef) {
	// Grab timer instance
	XTmrCtr *InstancePtr = (XTmrCtr *) CallbackRef;

	// Process interrupt
	if(usState == US_S_ADC_REQUEST) {
		// Change state
		usState = US_S_ADC_RESPONSE;

		// Compute conversion byte using channel
		unsigned char adcConversionByte = 0x86 | ((usSensorMap[usSensorIndex] & 0x0f) << 3);

		// Set slave select bit in hardware
		XSpi_SetSlaveSelectReg(&SpiADC, SpiADC.SlaveSelectReg);

		// Read control register
		u32 ControlReg = XSpi_ReadReg(SpiADC.BaseAddr, XSP_CR_OFFSET);

		// Mask off transmit inhibit bit
		ControlReg &= ~XSP_CR_TRANS_INHIBIT_MASK;

		// Load single byte into TX FIFO
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_DTR_OFFSET, adcConversionByte);

		// Start transfer by no longer inhibiting transmitter
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_CR_OFFSET, ControlReg);

		// Inhibit transmitter
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_CR_OFFSET, ControlReg | XSP_CR_TRANS_INHIBIT_MASK);

		// Clear RX FIFO
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_CR_OFFSET, ControlReg | XSP_CR_RXFIFO_RESET_MASK);
	} else {
		// Change state
		if(usState != US_S_ADC_ERROR_RESPONSE) {
			usState = US_S_ADC_ERROR_REQUEST;
		}

		// Stop sampling timer
		timer_setstate(InstancePtr, 0);
	}
}

void InterruptHandler_US_GPIO(void *CallbackRef) {
	// Grab GPIO instance
	XGpio *InstancePtr = (XGpio *) CallbackRef;

	// Check state
	if(usState == US_S_ADC_RESPONSE) {
		// Read control register
		u32 ControlReg = XSpi_ReadReg(SpiADC.BaseAddr, XSP_CR_OFFSET);

		// Load two nulls into TX FIFO
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_DTR_OFFSET, 0);
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_DTR_OFFSET, 0);

		// Start transfer by no longer inhibiting transmitter
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_CR_OFFSET, ControlReg);

		// Wait for transfer to complete
		while((XSpi_ReadReg(SpiADC.BaseAddr, XSP_SR_OFFSET) & XSP_SR_TX_EMPTY_MASK) == 0);

		// Inhibit transmitter
		ControlReg |= XSP_CR_TRANS_INHIBIT_MASK;
		XSpi_WriteReg(SpiADC.BaseAddr, XSP_CR_OFFSET, ControlReg);

		// Read RX FIFO
		unsigned short rx1 = XSpi_ReadReg(SpiADC.BaseAddr, XSP_DRR_OFFSET);
		unsigned short rx2 = XSpi_ReadReg(SpiADC.BaseAddr, XSP_DRR_OFFSET);

		// Prepare result
		unsigned short adcResult = (rx2 << 6) | (rx1 >> 2);

		// Store sample
		usWaveformData[usSensorMap[usSensorIndex]][usSampleIndex] = adcResult;

		// Increment sample index
		usSampleIndex++;

		// Check sample index, for sampling complete
		if(usSampleIndex == US_RX_COUNT) {
			// Stop sampling timer
			timer_setstate(&TimerUs, 0);

			// Check mode
			if(usMode == US_M_COMPLETE) {
				// Increment sensor index
				usSensorIndex++;

				// Check sensor index
				if(usSensorIndex == US_SENSOR_COUNT) {
					// Change state
					usState = US_S_COMPLETE;
				} else {
					// Start next request
					usarray_scan();
				}
			} else {
				// Change state
				usState = US_S_COMPLETE;
			}
		} else {
			// Change state
			usState = US_S_ADC_REQUEST;
		}
	} else {
		// Change state
		if(usState != US_S_ADC_ERROR_REQUEST) {
			usState = US_S_ADC_ERROR_RESPONSE;
		}
	}

	// Clear interrupt
	u32 GPIO_Reg = XGpio_ReadReg(InstancePtr->BaseAddress, XGPIO_ISR_OFFSET);
	XGpio_WriteReg(InstancePtr->BaseAddress, XGPIO_ISR_OFFSET, GPIO_Reg & 0x01);
}

void usarray_set_mode(enum US_MODE newMode) {
	// Select new mode
	usMode = newMode;
}

enum US_MODE usarray_get_mode() {
	// Return mode
	return usMode;
}

void usarray_set_sensor(unsigned char newSensor) {
	// Select new sensor
	usSensorIndex = newSensor;
}

unsigned char usarray_get_sensor() {
	// Return sensor
	return usSensorIndex;
}

void usarray_set_triggers(unsigned short changever, unsigned short nearLower, unsigned short nearUpper, unsigned short farLower, unsigned short farUpper) {
	// Update trigger levels
	usTriggerChangeIndex = USTimeToSampleIndex(changever);
	usTriggerNearLower = USVoltageToTriggerLevel(nearLower);
	usTriggerNearUpper = USVoltageToTriggerLevel(nearUpper);
	usTriggerFarUpper = USVoltageToTriggerLevel(farLower);
	usTriggerFarLower = USVoltageToTriggerLevel(farUpper);
}

enum US_STATE usarray_get_status() {
	// Return current status
	return usState;
}

void usarray_scan() {
	// Change state
	usState = US_S_ADC_REQUEST;

	// Reset sample index
	usSampleIndex = 0;

	// Start sampling timer
	timer_setstate(&TimerUs, 1);

	// Generate ultrasound pulse
	pulseGen_GeneratePulse(XPAR_AXI_PULSEGEN_US_BASEADDR, 1, usSensorMap[usSensorIndex], US_TX_COUNT);
}

void usarray_update_ranges() {
	// Compute speed of sound based on temperature
	unsigned int speedOfSound = (3313000 + 606 * usTemperature) / 10000; //mm/uS expressed in thousandths

	// Compute sensor start and end indexes based on mode
	unsigned char usSensorStartIndex = 0;
	unsigned char usSensorEndIndex = US_SENSOR_COUNT;
	if(usMode == US_M_SINGLE) {
		usSensorStartIndex = usSensorIndex;
		usSensorEndIndex = usSensorIndex + 1;
	}

	// Update range readings for each sensor
	int iSensor;
	int iSample;
	unsigned short triggerUpper;
	unsigned short triggerLower;
	for(iSensor = usSensorStartIndex; iSensor < usSensorEndIndex; iSensor++) {
		// Assume nothing will be found
		usRangeReadings[iSensor] = -1;

		// Example each sample
		for(iSample = 0; iSample < US_RX_COUNT; iSample++) {
			// Work out trigger levels for sample
			if(USSampleIndexToTime(iSample) <= usTriggerChangeIndex) {
				triggerUpper = usTriggerNearUpper;
				triggerLower = usTriggerNearLower;
			} else {
				triggerUpper = usTriggerFarUpper;
				triggerLower = usTriggerFarLower;
			}

			// Check sample against trigger levels
			if(usWaveformData[iSensor][iSample] <= triggerLower || usWaveformData[iSensor][iSample] >= triggerUpper) {
				// Update range reading - converting distance from thousandths of mm to mm and halving to retrieve one way distance
				usRangeReadings[iSensor] = (((unsigned int) USSampleIndexToTime(iSample)) * speedOfSound) / (1000 * 2);

				// Done
				break;
			}
		}
	}
}

void usarray_reset() {
	// Change state - timer will stop itself and / or ADC conversion result will be ignored - final state will likely be ERROR_ADC_*
	usState = US_S_IDLE;
}

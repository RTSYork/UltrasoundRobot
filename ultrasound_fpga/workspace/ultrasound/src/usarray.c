#include "usarray.h"

#include "xparameters.h"

#include "pulsegen.h"
#include "us_receiver.h"

#define USVoltageToTriggerLevel(x) (unsigned short) ((((unsigned int) x) * ((1 << USADCPrecision) - 1)) / USADCReference) // Voltage expressed in hundredths
#define USSampleIndexToTime(x) (unsigned short) ((((1000000 * 10) / US_SAMPLE_RATE) * (((unsigned int) x) + 1)) / 10) // Time expressed in uS
#define USTimeToSampleIndex(x) (unsigned short) ((((unsigned int) x) * 10) / ((1000000 * 10) / US_SAMPLE_RATE) - 1) // Time in uS

const unsigned char usSensorMap[] = US_SENSOR_MAP; // Sensor position to address map

enum US_MODE usMode = US_M_SINGLE; // Start in single sensor mode

unsigned char usSensorIndex = 0; // Next sensor to scan
unsigned short usSampleIndex = 0; // Sample index, incremented once per ADC conversion, representative of ToF
unsigned short usWaveformData[US_SENSOR_COUNT][US_RX_COUNT]; // Raw waveform data - stored as ADC results
signed short usRangeReadings[US_SENSOR_COUNT]; // Latest range readings - stored as sample indexes

unsigned short usTriggerChangeIndex = USTimeToSampleIndex(TRIGGER_NEAR_FAR_CHANGE); // Trigger changeover time
unsigned short usTriggerNearUpper = USVoltageToTriggerLevel(TRIGGER_BASE + TRIGGER_OFFSET_NEAR); // Upper trigger level
unsigned short usTriggerNearLower = USVoltageToTriggerLevel(TRIGGER_BASE - TRIGGER_OFFSET_NEAR); // Lower trigger level
unsigned short usTriggerFarUpper = USVoltageToTriggerLevel(TRIGGER_BASE + TRIGGER_OFFSET_FAR); // Upper trigger level
unsigned short usTriggerFarLower = USVoltageToTriggerLevel(TRIGGER_BASE - TRIGGER_OFFSET_FAR); // Lower trigger level

signed short usTemperature = 210; // Temperature in degrees C, expressed in tenths


int init_usarray() {
	// Reset all ranges
	int i;
	for(i = 0; i < US_SENSOR_COUNT; i++) usRangeReadings[i] = -1;

	// Setup ADC by writing to setup register (0x64)
	// Set to use internal clock for sampling and conversions, use external single ended reference
	sendUSInit();

	u8 status;
	u8 type;
	u32 data;
	readUSData(&status, &type, &data);

	if (status == US_STATUS_OK && type == US_RESP_NONE)
		return XST_SUCCESS;
	else
		return XST_FAILURE;
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

short usarray_get_temperature() {
	// Return temperature
	return usTemperature;
}

void usarray_measure_temp() {
	// Compute conversion byte, request temperature conversion and ignore single channel conversion (0xf9)
	sendUSTempRequest();

	// Read temperature back (blocking)
	u8 status;
	u8 type;
	u32 adcTempResult;
	readUSData(&status, &type, &adcTempResult);

	// This might not be right? Doing (temp*1.25) seems too high, but might just be warm chip.
	usTemperature = (((int) adcTempResult) * 125 * 10) / 1000;
}

void usarray_scan() {
	// Reset sample index
	usSampleIndex = 0;

	int minSensor = (usMode == US_M_SINGLE) ? usSensorIndex : 0;
	int maxSensor = (usMode == US_M_SINGLE) ? usSensorIndex + 1 : US_SENSOR_COUNT;
	int sensor;
	int sample;

	// Iterate through sensors
	for (sensor = minSensor; sensor < maxSensor; sensor++) {
		// Generate ultrasound pulse
		pulseGen_GeneratePulse(XPAR_AXI_PULSEGEN_US_BASEADDR, 1, usSensorMap[sensor], US_TX_COUNT);

		// Start sampling at 80kHz
		sendUSSampleRequest(usSensorMap[sensor], US_RX_COUNT, 1250);

		// Read all sample data
		for (sample = 0; sample < US_RX_COUNT; sample++) {
			u8 status;
			u8 type;
			u32 adcResult;
			readUSData(&status, &type, &adcResult);

			usWaveformData[sensor][sample] = adcResult;
		}
	}
}

void usarray_update_ranges() {
	// Compute speed of sound based on temperature
	unsigned int speedOfSound = (3313000 + 606 * usTemperature) / 10000; //mm/uS expressed in thousandths

	// Compute sensor start and end indexes based on mode
	unsigned char usSensorStartIndex = (usMode == US_M_SINGLE ? usSensorIndex : 0);
	unsigned char usSensorEndIndex = (usMode == US_M_SINGLE ? usSensorIndex + 1: US_SENSOR_COUNT);

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
			if(iSample <= usTriggerChangeIndex) {
				triggerUpper = usTriggerNearUpper;
				triggerLower = usTriggerNearLower;
			} else {
				triggerUpper = usTriggerFarUpper;
				triggerLower = usTriggerFarLower;
			}

			// Check sample against trigger levels
			if(usWaveformData[iSensor][iSample] <= triggerLower || usWaveformData[iSensor][iSample] >= triggerUpper) {
				// Update range reading - converting distance from thousandths of mm to mm and halving to retrieve one way distance
				usRangeReadings[iSensor] = ((((unsigned int) USSampleIndexToTime(iSample)) * speedOfSound) / (1000 * 2)) - 20;

				// Done
				break;
			}
		}
	}
}

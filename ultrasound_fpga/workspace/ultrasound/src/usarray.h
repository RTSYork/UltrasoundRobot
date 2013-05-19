#ifndef USARRAY_H_
#define USARRAY_H_

#include "timer.h"

enum US_MODE {
	US_M_SINGLE, // Scan single sensor once
	US_M_COMPLETE, // Scan complete array once
};

enum US_STATE {
	US_S_IDLE, // Scan not started yet
	US_S_ADC_REQUEST, // ADC conversion request needs to be made
	US_S_ADC_RESPONSE, // ADC conversion result needs to be fetched
	US_S_COMPLETE, // Scan complete
	US_S_ADC_ERROR_REQUEST, // ADC error has occurred, EOC flag not set before next conversion trigger time
	US_S_ADC_ERROR_RESPONSE // ADC error has occurred, EOC flag has occurred when not expected
};

#define US_SENSOR_COUNT 10 // Number of sensors installed on platform
#define US_SENSOR_MAP {9, 10, 11, 1, 2, 3, 4, 5, 6, 8} //Map sensor positions to sensor addresses
#define US_SAMPLE_RATE 35000 //80000 // Hz
#define US_RX_COUNT 5000 // Number of waveform samples to take at US_SAMPLE_RATE in a single ranging operation
#define US_TX_COUNT 8 // Cycles of 40Khz ultrasound to transmit

#define USADCPrecision 10 // Bits
#define USADCReference 330 // Volts - expressed in hundredths

extern XTmrCtr TimerUs; // Timer for ADC sampling, provide external access to connect interrupts

extern volatile unsigned short usWaveformData[US_SENSOR_COUNT][US_RX_COUNT]; // Provide external access to sample results
extern volatile signed short usRangeReadings[US_SENSOR_COUNT]; // Provide external access to range readings

int init_usarray();

void InterruptHandler_US_GPIO(void *CallbackRef);
void InterruptHandler_US_Timer(void *CallbackRef);

void usarray_setmode(enum US_MODE newMode);
void usarray_selectSensor(unsigned char newSensor);
enum US_MODE usarray_get_mode();
unsigned char usarray_get_sensor();
enum US_STATE usarray_get_status();
void usarray_scan();
void usarray_update_ranges();
void usarray_reset();

#endif /* USARRAY_H_ */

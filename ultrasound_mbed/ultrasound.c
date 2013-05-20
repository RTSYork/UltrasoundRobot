#include "ultrasound.h"
#include "led.h"
#include "serial.h"
#include "adc.h"
#include "pwm.h"
#include "systick.h"
#include "timer.h"

#include "lpc17xx_gpio.h"

volatile unsigned char txCounter;

volatile unsigned short samples[DATA_SIZE];
volatile unsigned short sampleIndex;
volatile unsigned char sampleComplete;

void main(void) {
	//Init hardware
	led_init();
	serial_init();
	adc_init();
	pwm_init();
	//systick_init();
	timer_init();

	//Welcome
	serial_printf("Compiled: %s %s Buffer Size: %i\n", __DATE__, __TIME__, DATA_SIZE);

	//Init TX debug line
	GPIO_SetDir(DEBUG_TX_PORT, (1 << DEBUG_TX_LINE), 1);
	GPIO_ClearValue(DEBUG_TX_PORT, (1 << DEBUG_TX_LINE));

	//Init RX debug line
	GPIO_SetDir(DEBUG_RX_PORT, (1 << DEBUG_RX_LINE), 1);
	GPIO_ClearValue(DEBUG_RX_PORT, (1 << DEBUG_RX_LINE));

	//Init TX enable line
	GPIO_SetDir(TX_ENABLE_PORT, (1 << TX_ENABLE_LINE), 1);
	GPIO_ClearValue(TX_ENABLE_PORT, (1 << TX_ENABLE_LINE));

	//Enable LED
	led_write(0, 1);

	while(1) {
		//Flash Debug LED
		led_write(3, !led_read(3));
        
		//Reset sample index and clear complete flag
		sampleIndex = 0;
		sampleComplete = 0;

		//Restart transmitter counter (timer ticks every 10uS)
		txCounter = TRANSMIT_TIME_US / 10;

		//Set TX debug pin
		//GPIO_SetValue(DEBUG_TX_PORT, (1 << DEBUG_TX_LINE));

		//Set TX enable pin
		GPIO_SetValue(TX_ENABLE_PORT, (1 << TX_ENABLE_LINE));

		//Start ultrasound transmitter
		pwm_enable();

		//Start receiver timer
		timer_start(1);

		//Start transmitter timer
		timer_start(0);

		//Wait for ADC sampling to complete
		while(!sampleComplete);

	        //Send result
		unsigned short i;
	        serial_printf("START\n");
	        for(i = 0; i < DATA_SIZE; i++) serial_printf("0,%i\n", samples[i]);
	        serial_printf("END\n");

		//Wait a while
		//systick_sleep(100);
	}
}

void TIMER0_IRQHandler(void) {
	//Clear match 0 interrupt flag
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);

	//Decrement counter
	txCounter--;

	//Check counter
	if(txCounter == 0) {
		//Stop transmitter
		pwm_disable();

		//Clear TX enable pin
		GPIO_ClearValue(TX_ENABLE_PORT, (1 << TX_ENABLE_LINE));

		//Clear TX debug pin
		//GPIO_ClearValue(DEBUG_TX_PORT, (1 << DEBUG_TX_LINE));

		//Stop timer
		timer_stop(0);
	}
}

void TIMER1_IRQHandler(void) {
	//Set RX debug pin
	//GPIO_SetValue(DEBUG_RX_PORT, (1 << DEBUG_RX_LINE));

	//Sample ADC
	samples[sampleIndex] = adc_read();

	//Increment index
	sampleIndex++;

	//Check for completion, set flag and stop timer
	if(sampleIndex == DATA_SIZE) {
		//Set flag
		sampleComplete = 1;

		//Stop timer
		timer_stop(1);
	}

	//Clear match 0 interrupt flag
	TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);

	//Clear RX debug pin
	//GPIO_ClearValue(DEBUG_RX_PORT, (1 << DEBUG_RX_LINE));
}

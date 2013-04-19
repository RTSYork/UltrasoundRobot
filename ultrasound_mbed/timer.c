#include "timer.h"

void timer_init() {
	TIM_TIMERCFG_Type TIM_ConfigStruct;
	TIM_MATCHCFG_Type TIM_MatchConfigStruct;

	//Set timer prescaler to 0 (increment every tick)
	TIM_ConfigStruct.PrescaleOption = TIM_PRESCALE_TICKVAL;
	TIM_ConfigStruct.PrescaleValue = 1;

	//Init timer 0
	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &TIM_ConfigStruct);

	//Init timer 1
	TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &TIM_ConfigStruct);

	//Configure match 0
	TIM_MatchConfigStruct.MatchChannel = 0;
	TIM_MatchConfigStruct.IntOnMatch = TRUE;
	TIM_MatchConfigStruct.ResetOnMatch = TRUE;
	TIM_MatchConfigStruct.StopOnMatch  = FALSE;
	TIM_MatchConfigStruct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;

	//Set match 0 on timer 0 (10uS @ 24Mhz)
	TIM_MatchConfigStruct.MatchValue = 240;
	TIM_ConfigMatch(LPC_TIM0, &TIM_MatchConfigStruct);

	//Set match 0 on timer 1 (12.5uS @ 24Mhz)
	TIM_MatchConfigStruct.MatchValue = 300;
	TIM_ConfigMatch(LPC_TIM1, &TIM_MatchConfigStruct);

	//Set timer 0 interrupt priority
	NVIC_SetPriority(TIMER0_IRQn, ((0x01<<3)|0x01));

	//Set timer 1 interrupt priority
	NVIC_SetPriority(TIMER1_IRQn, ((0x01<<3)|0x02));
	
	//Enable interrupt for timer 0
	NVIC_EnableIRQ(TIMER0_IRQn);

	//Enable interrupt for timer 1
	NVIC_EnableIRQ(TIMER1_IRQn);
}

void timer_start(uint8_t num) {
	switch(num) {
		case 0:
			TIM_ResetCounter(LPC_TIM0);
			TIM_Cmd(LPC_TIM0, ENABLE);
			break;
		case 1:
			TIM_ResetCounter(LPC_TIM1);
			TIM_Cmd(LPC_TIM1, ENABLE);
			break;
		default:
			break;
	}
}

void timer_stop(uint8_t num) {
	switch(num) {
		case 0:
			TIM_Cmd(LPC_TIM0, DISABLE);
			break;
		case 1:
			TIM_Cmd(LPC_TIM1, DISABLE);
			break;
		default:
			break;
	}
}

#include "pwm.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_pwm.h"

void pwm_init() {
	PINSEL_CFG_Type PinCfg;
	PWM_TIMERCFG_Type PWMCfgDat;
	PWM_MATCHCFG_Type PWMMatchCfgDat;

	//Init pin - P26
	PinCfg.Funcnum = PINSEL_FUNC_1;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	PinCfg.Portnum = PINSEL_PORT_2;
	PinCfg.Pinnum = PINSEL_PIN_0;
	PINSEL_ConfigPin(&PinCfg);

	//Initialize PWM peripheral
	PWMCfgDat.PrescaleOption = PWM_TIMER_PRESCALE_TICKVAL;
	PWMCfgDat.PrescaleValue = 1;
	PWM_Init(LPC_PWM1, PWM_MODE_TIMER, (void *) &PWMCfgDat);

	//Set match value for PWM match channel 0 - 40Khz
	PWM_MatchUpdate(LPC_PWM1, 0, 600, PWM_MATCH_UPDATE_NOW);
	PWMMatchCfgDat.IntOnMatch = DISABLE;
	PWMMatchCfgDat.MatchChannel = 0;
	PWMMatchCfgDat.ResetOnMatch = ENABLE;
	PWMMatchCfgDat.StopOnMatch = DISABLE;
	PWM_ConfigMatch(LPC_PWM1, &PWMMatchCfgDat);
	 
	//Configure match value for PWM match channel 1 - 50% duty
	PWM_MatchUpdate(LPC_PWM1, 1, 300, PWM_MATCH_UPDATE_NOW);
	PWMMatchCfgDat.IntOnMatch = DISABLE;
	PWMMatchCfgDat.MatchChannel = 1;
	PWMMatchCfgDat.ResetOnMatch = DISABLE;
	PWMMatchCfgDat.StopOnMatch = DISABLE;
	PWM_ConfigMatch(LPC_PWM1, &PWMMatchCfgDat);

	//Reset counter
	PWM_ResetCounter(LPC_PWM1);

	//Start PWM
	PWM_Cmd(LPC_PWM1, ENABLE);
}

void pwm_enable() {
	//Enable PWM on match channel 1
	PWM_ChannelCmd(LPC_PWM1, 1, ENABLE);

	//Start counter
	PWM_CounterCmd(LPC_PWM1, ENABLE);
}

void pwm_disable() {
	//Disable PWM on match channel 1
	PWM_ChannelCmd(LPC_PWM1, 1, DISABLE);

	//Stop counter
	PWM_CounterCmd(LPC_PWM1, DISABLE);

	//Reset counter
	PWM_ResetCounter(LPC_PWM1);
}

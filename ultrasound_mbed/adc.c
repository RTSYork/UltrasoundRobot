#include "adc.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_adc.h"

//http://www-users.cs.york.ac.uk/~pcc/MCP/MBed_MCP/CMSIS/examples/html/adc__interrupt__test_8c_source.html

//ADC is only 12bit

void adc_init() {
	//Init pin - P15
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = PINSEL_FUNC_1;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	PinCfg.Portnum = PINSEL_PORT_0;
	PinCfg.Pinnum = PINSEL_PIN_23;
	PINSEL_ConfigPin(&PinCfg);
	
	//Set up the ADC conversion frequency
	ADC_Init(LPC_ADC, 200000);

	//Disable interrupt
	ADC_IntConfig(LPC_ADC, ADC_ADINTEN0, DISABLE);
	
	//Enable ADC channel 0
	ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_0, ENABLE);
}

uint16_t adc_read() {
	//Start ADC conversion
	ADC_StartCmd(LPC_ADC, ADC_START_NOW);

	//Wait for conversion to complete
	while(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_0, ADC_DATA_DONE) != SET);

	//Return value
	return ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_0);
}

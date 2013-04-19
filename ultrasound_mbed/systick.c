#include "systick.h"

#include "lpc17xx_systick.h"

//System tick count
volatile unsigned long SysTickCnt;

//Handler prototype
void SysTick_Handler(void);

void systick_init() {
	//Configure system tick counter, generate tick ever 1ms
	SysTick_Config(SystemCoreClock / 1000 - 1);
}

void SysTick_Handler(void) {
	//Increment system tick counter
	SysTickCnt++;
}

void systick_sleep(unsigned long tick) {
	unsigned long target;

	//Calculate time to wait until
	target = SysTickCnt + tick;

	//Wait for clock to advance
	while(SysTickCnt < target);
}

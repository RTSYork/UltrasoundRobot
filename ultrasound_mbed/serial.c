#include "serial.h"

#include <stdio.h>
#include <stdarg.h>

#include "lpc17xx_pinsel.h"
#include "lpc17xx_uart.h"

void serial_init(void) {
	PINSEL_CFG_Type PinCfg;
	UART_CFG_Type UARTConfigStruct;
	UART_FIFO_CFG_Type UARTFIFOConfigStruct;

	//Init pins
	PinCfg.Funcnum = PINSEL_FUNC_1;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	PinCfg.Portnum = PINSEL_PORT_0;
	PinCfg.Pinnum = PINSEL_PIN_2;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = PINSEL_PIN_3;
	PINSEL_ConfigPin(&PinCfg);
		
	//Init UART Configuration parameter structure to default state: 115200,8,1,N
	UART_ConfigStructInit(&UARTConfigStruct);
	UARTConfigStruct.Baud_rate = 115200;

	//Init FIFOConfigStruct using defaults
	UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);

	//Start the devices USB serial
	UART_Init((LPC_UART_TypeDef *)LPC_UART0, &UARTConfigStruct);
	UART_FIFOConfig((LPC_UART_TypeDef *)LPC_UART0, &UARTFIFOConfigStruct);
	UART_TxCmd((LPC_UART_TypeDef *)LPC_UART0, ENABLE);
}

int serial_read(char *buf, int length) {
	return(UART_Receive((LPC_UART_TypeDef *)LPC_UART0, (uint8_t *)buf, length, NONE_BLOCKING));
}

int serial_write(char *buf, int length) {
	return(UART_Send((LPC_UART_TypeDef *)LPC_UART0, (uint8_t *)buf, length, BLOCKING));
}

int serial_printf(const char *format, ...) {
	va_list arg;
	char buf[100];
	int done;

	va_start(arg, format);
	done = vsprintf((char*) &buf, format, arg);
	va_end(arg);

	if(done > 0) serial_write((char*) &buf, done);

	return done;
}

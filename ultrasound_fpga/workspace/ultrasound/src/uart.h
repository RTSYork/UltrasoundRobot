#ifndef UART_H_
#define UART_H_

#include "xuartlite.h"
#include "xuartlite_l.h"

#define BUFFER_SIZE_TX 128
#define BUFFER_SIZE_RX 128

typedef struct uart_buff {
	XUartLite uart;

	char *bufferTX;
	int sizeTX;
	int indexTXRead;
	int indexTXWrite;
	int countTX;
	char overflowTX;

	char *bufferRX;
	int sizeRX;
	int indexRXRead;
	int indexRXWrite;
	int countRX;
	char overflowRX;
} uart_buff;

int init_uart_buffers(int deviceID, volatile uart_buff *uart_buf);

void InterruptHandler_UART(void *CallbackRef);

int uart_getchar(volatile uart_buff *buf);
int uart_putchar(volatile uart_buff *buf, char c);
int uart_print(volatile uart_buff *buf, char* str);

#endif /* UART_H_ */

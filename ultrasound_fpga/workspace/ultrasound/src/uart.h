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
	volatile int indexTXRead;
	volatile int indexTXWrite;
	volatile int countTX;
	volatile char overflowTX;

	char *bufferRX;
	int sizeRX;
	volatile int indexRXRead;
	volatile int indexRXWrite;
	volatile int countRX;
	volatile char overflowRX;
} uart_buff;

int init_uart_buffers(int deviceID, uart_buff *uart_buf);

void InterruptHandler_UART(void *CallbackRef);

int uart_getchar(uart_buff *buf);
int uart_putchar(uart_buff *buf, char c);

int get_tx_count(uart_buff *buf);
int get_rx_count(uart_buff *buf);

int uart_print(uart_buff *buf, char* str);

int uart_print_int(uart_buff *buf, int val, unsigned char isSigned);

int uart_print_char(uart_buff *buf, char c);

#endif /* UART_H_ */

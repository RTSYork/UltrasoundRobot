#include "uart.h"

#include <stdlib.h>

int init_uart_buffers(int deviceID, uart_buff *uart_buf) {
	// Init UART
	if(XUartLite_Initialize((XUartLite*) &(uart_buf->uart), deviceID) != XST_SUCCESS) return XST_FAILURE;

	// Self test - cannot be performed as the UART will have already been used by print
	// if(XUartLite_SelfTest(uart) != XST_SUCCESS) return XST_FAILURE;

	// Setup UART buffers
	uart_buf->bufferRX = malloc(BUFFER_SIZE_RX);
	if(uart_buf->bufferRX == NULL) return XST_FAILURE;
	uart_buf->sizeRX = BUFFER_SIZE_RX;
	uart_buf->countRX = 0;
	uart_buf->bufferTX = malloc(BUFFER_SIZE_TX);
	if(uart_buf->bufferTX == NULL) return XST_FAILURE;
	uart_buf->sizeTX = BUFFER_SIZE_TX;
	uart_buf->countTX = 0;

	// Enable UART interrupts
	XUartLite_EnableInterrupt((XUartLite*) &(uart_buf->uart));

	// Yey!
	return XST_SUCCESS;
}

void InterruptHandler_UART(void *CallbackRef) {
	// Grab UART buffer
	uart_buff *buf = (uart_buff*) CallbackRef;

	// Read UART status
	int IsrStatus = XUartLite_GetStatusReg(buf->uart.RegBaseAddress);

	// RX FIFO not empty?
	while(IsrStatus & XUL_SR_RX_FIFO_VALID_DATA) {
		// Check RX buffer has space
		if(buf->countRX < buf->sizeRX) {
			// Read byte
			char c = XUartLite_RecvByte(buf->uart.RegBaseAddress);

			// Add to buffer, advance pointer and increment count
			buf->bufferRX[buf->indexRXWrite] = c;
			buf->indexRXWrite++;
			if(buf->indexRXWrite == buf->sizeRX) buf->indexRXWrite = 0;
			buf->countRX++;
		} else {
			// Set overflow flag if RX FIFO full
			if(IsrStatus & XUL_SR_RX_FIFO_FULL) buf->overflowRX = 1;
		}

		// Update status register
		IsrStatus = XUartLite_GetStatusReg(buf->uart.RegBaseAddress);
	}

	// TX FIFO empty?
	if(IsrStatus & XUL_SR_TX_FIFO_EMPTY) {
		// Send while TX buffer has data and TX FIFO not full
		while((!(IsrStatus & XUL_SR_TX_FIFO_FULL)) && (buf->countTX > 0)) {
			// Send byte, advance pointer and decrement count
			XUartLite_WriteReg(buf->uart.RegBaseAddress, XUL_TX_FIFO_OFFSET, buf->bufferTX[buf->indexTXRead]);
			buf->indexTXRead++;
			if(buf->indexTXRead == buf->sizeTX) buf->indexTXRead = 0;
			buf->countTX--;

			// Update status register
			IsrStatus = XUartLite_GetStatusReg(buf->uart.RegBaseAddress);
		}
	}
}

int uart_getchar(uart_buff *buf) {
	unsigned char c;

	// Check count
	if(buf->countRX > 0) {
		// Retrieve character, advance pointer and decrement count
		c = buf->bufferRX[buf->indexRXRead];
		buf->indexRXRead++;
		if(buf->indexRXRead == buf->sizeRX) buf->indexRXRead = 0;
		buf->countRX--;
	} else {
		// Oh noes
		return -1;
	}

	// Return character
	return (int) c;
}

int uart_putchar(uart_buff *buf, char c) {
	// Check count
	if(buf->countTX < buf->sizeTX) {
		buf->bufferTX[buf->indexTXWrite] = c;
		buf->indexTXWrite++;
		if(buf->indexTXWrite == buf->sizeTX) buf->indexTXWrite = 0;
		buf->countTX++;
	} else {
		// Set overflow flag
		buf->overflowTX = 1;

		// Oh noes
		return -1;
	}

	// Send first byte if TX FIFO empty - interrupt handler will do the rest :-)
	if(XUartLite_GetStatusReg(buf->uart.RegBaseAddress) & XUL_SR_TX_FIFO_EMPTY) {
		// Disable interrupts
		//XUartLite_DisableIntr(buf->uart.RegBaseAddress);

		// Bit naughty, call interrupt handler to send first byte
		InterruptHandler_UART((void*) buf);

		// Enable interrupts
		//XUartLite_EnableIntr(buf->uart.RegBaseAddress);
	}

	// Yey!
	return 1;
}

int get_tx_count(uart_buff *buf) {
	// Return TX buffer byte count
	return buf->countTX;
}

int get_rx_count(uart_buff *buf) {
	// Return RX buffer byte count
	return buf->countRX;
}

int uart_print(uart_buff *buf, char* str) {
	// Send characters until null terminator reached
	while (*str != '\0') {
		// Print character, retying on buffer full
		while(uart_putchar(buf, *str) == -1);

		// Increment pointer
		str++;
	}

	// Yey!
	return 1;
}

int uart_print_int(uart_buff *buf, int val, unsigned char isSigned) {
    // Handle sign
	if(val < 0 && isSigned) {
	    // Output sign
	    while(uart_putchar(buf, '-') == -1);

	    // Remove sign
	    val = -val;
    }

    // Handle zero
    if(val == 0) {
        // Output digit
    	while(uart_putchar(buf, '0') == -1);
    } else {
        // Generate string
        char str[12]; // Allocate enough memory for max integer length and null terminator
        char *ptr = &str[11]; // Setup pointer at end of string
        *ptr = '\0'; // Null terminate string
	    while(val > 0) {
		    // Generate character
		    *--ptr = '0' + (val % 10);

		    // Shift number
		    val /= 10;
	    }

	    // Print string
	    uart_print(buf, ptr);
    }

	// Yey!
	return 1;
}

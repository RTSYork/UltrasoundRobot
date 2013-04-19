#ifndef ULTRASOUND_H
#define ULTRASOUND_H

#include "lpc_types.h"

#define DEBUG_TX_PORT 2
#define DEBUG_TX_LINE 5 //P21

#define DEBUG_RX_PORT 2
#define DEBUG_RX_LINE 4 //P22

#define TX_ENABLE_PORT 2
#define TX_ENABLE_LINE 1 //P25

#define TRANSMIT_TIME_US 100
#define RECEIVE_TIME_US 4000

#define DATA_SIZE (RECEIVE_TIME_US / ((1000000 * 10) / 80000)) * 10 //Divide and muliply by 10 to maintain precision

void main(void);

#endif

#pragma once

#include <stdint.h>

#include "air105.h"

#define UART_IT_RX_RECVD (UART_IER_ERBFI)
#define UART_IT_TX_EMPTY (UART_IER_ETBEI)
#define UART_IT_LINE_STATUS (UART_IER_ELSI)
#define UART_IT_MODEM_STATUS (UART_IER_EDSSI)

uint8_t SerialGetByte(void);
void SerialSend(uint8_t* buf, uint16_t len);
uint8_t SerialIsRXEmpty(void);

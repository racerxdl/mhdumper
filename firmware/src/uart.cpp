#include "uart.h"

extern "C" {
#include "air105.h"
#include "air105_otp.h"
#include "air105_spi.h"
#include "delay.h"
#include "irq.h"
#include "sysc.h"
}

#include <cstdio>

#define MAX_BUFFER_SIZE (4096)

struct {
    uint8_t rx_flag;
    uint16_t head;
    uint16_t tail;
    uint8_t Rx_buffer[MAX_BUFFER_SIZE];
} uart;

void push(uint8_t data) {
    uart.Rx_buffer[uart.head] = data;

    uart.head++;

    if (uart.head >= MAX_BUFFER_SIZE) {
        uart.head = 0;
    }
}

uint8_t SerialGetByte(void) {
    uint8_t data = uart.Rx_buffer[uart.tail];

    uart.tail++;

    if (uart.tail >= MAX_BUFFER_SIZE) {
        uart.tail = 0;
    }

    return data;
}

uint8_t SerialIsRXEmpty(void) {
    return uart.head == uart.tail;
}

void SerialSend(uint8_t* buf, uint16_t len) {
    uint16_t i = 0;

    for (i = 0; i < len; i++) {
        while (!UART_IsTXEmpty(UART0));
        UART_SendData(UART0, (uint8_t)buf[i]);
    }
}

uint8_t tmpRX = 0;

// UART0 interrupt handler
void UART0_IRQHandler(void) {
    volatile uint32_t iir;

    UART_TypeDef* UARTx = UART0;

    iir = UART_GetITIdentity(UARTx);
    switch (iir & 0x0f) {
        case UART_IT_ID_RX_RECVD: {
            tmpRX = UART_ReceiveData(UARTx);
            push(tmpRX);
        } break;
        default:
            break;
    }
}
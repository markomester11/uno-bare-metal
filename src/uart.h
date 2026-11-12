/*
 * Interrupt-driven UART0 driver for the ATmega328P (8N1).
 * Received bytes are queued by the RX interrupt; transmitted bytes are queued
 * and sent by the "data register empty" interrupt, so the CPU never busy-waits
 * on the serial line unless the TX buffer is full.
 */
#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stdint.h>

void uart_init(void);

/* Non-blocking: returns false if no byte has arrived. */
bool uart_getc(uint8_t *c);
bool uart_rx_available(void);

/* Blocks only while the TX buffer is full. Never call from an ISR. */
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_u32(uint32_t value);

/* Bytes lost because the RX buffer was full (saturates at 255). */
uint8_t uart_rx_overflows(void);

#endif /* UART_H */

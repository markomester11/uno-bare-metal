#include "uart.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "ringbuf.h"

#ifndef BAUD
#define BAUD 115200UL
#endif

/*
 * Double-speed mode (U2X0): UBRR = F_CPU / (8 * BAUD) - 1, rounded to nearest.
 * At 16 MHz and 115200 baud this gives UBRR = 16, i.e. 117647 baud (+2.1 %),
 * which is inside the tolerance of the Uno's USB-serial chip. Without U2X0 the
 * error would be -3.5 %, which is too high.
 */
#define UBRR_VALUE ((F_CPU + 4UL * BAUD) / (8UL * BAUD) - 1UL)

static ringbuf_t rx_buf;
static ringbuf_t tx_buf;
static volatile uint8_t rx_overflow_count;

void uart_init(void)
{
    rb_init(&rx_buf);
    rb_init(&tx_buf);

    UBRR0 = (uint16_t)UBRR_VALUE;
    UCSR0A = _BV(U2X0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);               /* 8 data bits, no parity, 1 stop bit */
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);   /* enable RX, TX and the RX interrupt */
}

/* A byte arrived. Reading UDR0 also clears the interrupt flag. */
ISR(USART_RX_vect)
{
    uint8_t c = UDR0;
    if (!rb_put(&rx_buf, c) && rx_overflow_count < 255u) {
        rx_overflow_count++;
    }
}

/* The transmit register is free: send the next queued byte, or stop the interrupt. */
ISR(USART_UDRE_vect)
{
    uint8_t c;
    if (rb_get(&tx_buf, &c)) {
        UDR0 = c;
    } else {
        UCSR0B &= (uint8_t)~_BV(UDRIE0);
    }
}

bool uart_getc(uint8_t *c)
{
    return rb_get(&rx_buf, c);
}

bool uart_rx_available(void)
{
    return !rb_empty(&rx_buf);
}

void uart_putc(char c)
{
    while (!rb_put(&tx_buf, (uint8_t)c)) {
        /* TX buffer full: the UDRE interrupt is draining it. */
    }
    /* Make sure the UDRE interrupt runs. If it has just switched itself off,
     * setting the bit again is harmless: it finds the new byte and sends it. */
    UCSR0B |= _BV(UDRIE0);
}

void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

void uart_put_u32(uint32_t value)
{
    char digits[10]; /* 4294967295 has 10 digits */
    uint8_t n = 0;
    do {
        digits[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (n > 0u) {
        uart_putc(digits[--n]);
    }
}

uint8_t uart_rx_overflows(void)
{
    return rx_overflow_count;
}

/*
 * Bare-metal firmware for the ATmega328P on an Arduino Uno.
 * No Arduino core and no libraries beyond avr-libc: every peripheral is set up
 * by writing its registers directly.
 *
 *   - Timer1 in CTC mode gives a 1 ms tick and drives LED blinking.
 *   - UART0 is interrupt-driven with ring buffers (see uart.c).
 *   - INT0 (pin D2) reacts to a button, debounced in software.
 *   - The CPU sleeps in idle mode whenever there is nothing to do.
 *   - A small command shell runs over the serial line.
 */
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <util/atomic.h>

#include "uart.h"

#define LED_BIT PB5          /* Arduino pin D13: the on-board "L" LED */
#define BUTTON_BIT PD2       /* Arduino pin D2 = INT0: touch a wire from D2 to GND */
#define DEBOUNCE_MS 50u
#define DEFAULT_BLINK_MS 500u
#define MAX_BLINK_MS 60000u
#define LINE_MAX 32u

static volatile uint32_t ms_ticks;
static volatile uint16_t blink_period_ms;   /* 0 = not blinking */
static volatile bool button_event;
static uint16_t last_blink_period_ms = DEFAULT_BLINK_MS;
static uint16_t button_presses;

/* ---------- LED ---------- */

static void led_on(void) { PORTB |= _BV(LED_BIT); }
static void led_off(void) { PORTB &= (uint8_t)~_BV(LED_BIT); }
static bool led_is_on(void) { return (PORTB & _BV(LED_BIT)) != 0; }

/* Writing a 1 to a PINx bit toggles the output in a single instruction. */
static void led_toggle(void) { PINB = _BV(LED_BIT); }

/* ---------- Timer1: 1 ms tick ---------- */

static void timer1_init(void)
{
    /* CTC mode, prescaler 64: 16 MHz / 64 = 250 kHz, so 250 counts = 1 ms. */
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
    OCR1A = 249;
    TIMSK1 = _BV(OCIE1A);
}

ISR(TIMER1_COMPA_vect)
{
    static uint16_t blink_count;

    ms_ticks++;
    if (blink_period_ms != 0u && ++blink_count >= blink_period_ms) {
        blink_count = 0;
        led_toggle();
    }
}

/* A 32-bit read takes four instructions, so block the tick ISR meanwhile. */
static uint32_t millis(void)
{
    uint32_t t;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = ms_ticks; }
    return t;
}

static void set_blink(uint16_t period_ms)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { blink_period_ms = period_ms; }
    if (period_ms != 0u) {
        last_blink_period_ms = period_ms;
    }
}

static uint16_t get_blink(void)
{
    uint16_t p;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { p = blink_period_ms; }
    return p;
}

/* ---------- Button on INT0 ---------- */

static void button_init(void)
{
    DDRD &= (uint8_t)~_BV(BUTTON_BIT);   /* input */
    PORTD |= _BV(BUTTON_BIT);            /* internal pull-up: reads 1 until shorted to GND */
    EICRA = _BV(ISC01);                  /* interrupt on the falling edge */
    EIFR = _BV(INTF0);                   /* clear a stale flag */
    EIMSK = _BV(INT0);
}

ISR(INT0_vect)
{
    static uint32_t last_press_ms;
    uint32_t now = ms_ticks; /* interrupts are off inside an ISR, so this read is safe */

    if (now - last_press_ms >= DEBOUNCE_MS) {
        last_press_ms = now;
        button_event = true;
    }
}

/* ---------- Command shell ---------- */

static void prompt(void) { uart_puts("> "); }

static void print_status(void)
{
    uint16_t period = get_blink();

    uart_puts("led: ");
    if (period != 0u) {
        uart_puts("blinking every ");
        uart_put_u32(period);
        uart_puts(" ms");
    } else {
        uart_puts(led_is_on() ? "on" : "off");
    }
    uart_puts("\r\nuptime: ");
    uart_put_u32(millis());
    uart_puts(" ms\r\nbutton presses: ");
    uart_put_u32(button_presses);
    uart_puts("\r\nrx overflows: ");
    uart_put_u32(uart_rx_overflows());
    uart_puts("\r\n");
}

static void handle_line(char *line)
{
    char *cmd = strtok(line, " ");
    char *arg = strtok(NULL, " ");

    if (cmd == NULL) {
        return;
    }
    if (strcmp(cmd, "help") == 0) {
        uart_puts("commands:\r\n"
                  "  led on | led off | led toggle\r\n"
                  "  blink <ms>   blink the LED (1-60000 ms), 'blink 0' stops\r\n"
                  "  uptime       milliseconds since reset\r\n"
                  "  status       LED, uptime, button and UART counters\r\n"
                  "  touch D2 to GND to start or stop blinking\r\n");
    } else if (strcmp(cmd, "led") == 0 && arg != NULL) {
        set_blink(0);
        if (strcmp(arg, "on") == 0) {
            led_on();
        } else if (strcmp(arg, "off") == 0) {
            led_off();
        } else if (strcmp(arg, "toggle") == 0) {
            led_toggle();
        } else {
            uart_puts("usage: led on | led off | led toggle\r\n");
            return;
        }
        uart_puts("ok\r\n");
    } else if (strcmp(cmd, "blink") == 0 && arg != NULL) {
        char *end;
        long ms = strtol(arg, &end, 10);
        if (*end != '\0' || ms < 0 || ms > (long)MAX_BLINK_MS) {
            uart_puts("usage: blink <0-60000>\r\n");
            return;
        }
        set_blink((uint16_t)ms);
        if (ms == 0) {
            led_off();
        }
        uart_puts("ok\r\n");
    } else if (strcmp(cmd, "uptime") == 0) {
        uart_put_u32(millis());
        uart_puts(" ms\r\n");
    } else if (strcmp(cmd, "status") == 0) {
        print_status();
    } else {
        uart_puts("unknown command, type 'help'\r\n");
    }
}

/* Line editing: echo, backspace, and Enter sent as CR, LF or CR+LF. */
static void handle_char(uint8_t c)
{
    static char line[LINE_MAX];
    static uint8_t len;
    static bool last_was_cr;

    if (c == '\n' && last_was_cr) {
        last_was_cr = false;
        return; /* second half of CR+LF */
    }
    last_was_cr = (c == '\r');

    if (c == '\r' || c == '\n') {
        uart_puts("\r\n");
        line[len] = '\0';
        handle_line(line);
        len = 0;
        prompt();
    } else if (c == 0x08u || c == 0x7Fu) { /* backspace or delete */
        if (len > 0u) {
            len--;
            uart_puts("\b \b");
        }
    } else if (c >= 0x20u && c < 0x7Fu) {
        if (len < LINE_MAX - 1u) {
            line[len++] = (char)c;
            uart_putc((char)c);
        } else {
            uart_putc('\a'); /* line full: ring the terminal bell */
        }
    }
}

/* ---------- Main loop ---------- */

int main(void)
{
    DDRB |= _BV(LED_BIT);
    uart_init();
    timer1_init();
    button_init();
    set_sleep_mode(SLEEP_MODE_IDLE); /* timers and UART keep running in idle */
    sei();

    uart_puts("\r\nATmega328P bare-metal shell. Type 'help'.\r\n");
    prompt();

    for (;;) {
        uint8_t c;
        while (uart_getc(&c)) {
            handle_char(c);
        }

        if (button_event) {
            button_event = false;
            button_presses++;
            if (get_blink() != 0u) {
                set_blink(0);
                led_off();
                uart_puts("\r\n[button] blinking stopped\r\n");
            } else {
                set_blink(last_blink_period_ms);
                uart_puts("\r\n[button] blinking every ");
                uart_put_u32(last_blink_period_ms);
                uart_puts(" ms\r\n");
            }
            prompt();
        }

        /* Sleep until the next interrupt (1 ms tick, UART byte or button).
         * Interrupts are off during the check; SEI enables them only after the
         * next instruction, so SLEEP runs first and no wake-up is lost. */
        cli();
        if (!uart_rx_available() && !button_event) {
            sleep_enable();
            sei();
            sleep_cpu();
            sleep_disable();
        } else {
            sei();
        }
    }
}

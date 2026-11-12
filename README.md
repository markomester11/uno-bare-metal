# Bare-metal ATmega328P shell

Firmware for the Arduino Uno's ATmega328P written in plain C with no Arduino core: every peripheral is configured by writing its registers directly. It runs a small command shell over USB serial, blinks the on-board LED from a timer interrupt and reacts to a button on an external interrupt.

## What it shows

- **Timer1 in CTC mode** generating a 1 ms tick (16 MHz / 64 prescaler / 250 counts) that drives LED blinking.
- **Interrupt-driven UART** at 115200 baud in double-speed mode (UBRR = 16, +2.1 % error), with RX and TX ring buffers so the CPU never waits on the serial line.
- **Lock-free single-producer/single-consumer ring buffer** with free-running 8-bit indices and a compiler barrier, unit-tested on the host.
- **External interrupt INT0** on pin D2 with the internal pull-up and 50 ms software debouncing.
- **Atomic access** to multi-byte variables shared with interrupts (`ATOMIC_BLOCK`).
- **Idle sleep** whenever there is nothing to do, using the `cli` / check / `sei; sleep` pattern so no wake-up is lost.
- **CI** on GitHub Actions: host unit tests plus a full AVR build on every push.

## Hardware

- Arduino Uno (ATmega328P, 16 MHz)
- One jumper wire: touch it from **D2** to **GND** to act as a button

The LED is the on-board "L" LED on D13 (PB5).

## Build and flash

You need `avr-gcc` and `avrdude`. Either:

- install **Arduino IDE 2** and select the Uno once (the Makefile finds its bundled toolchain automatically), or
- `brew install avrdude` and `brew tap osx-cross/avr && brew install avr-gcc`

```sh
make            # build build/firmware.hex and print its size
make flash      # upload over USB (finds /dev/cu.usbmodem* automatically)
make monitor    # open the serial shell at 115200 baud (quit: Ctrl-A, K, Y)
make test       # run the ring buffer unit tests on your computer
```

If the port isn't found: `make flash PORT=/dev/cu.usbmodemXXXX`. Older Uno clones may need `UPLOAD_BAUD=57600`.

## Shell commands

| Command | What it does |
| --- | --- |
| `help` | List commands |
| `led on` / `led off` / `led toggle` | Set the LED (stops blinking) |
| `blink <ms>` | Blink every 1–60000 ms; `blink 0` stops |
| `uptime` | Milliseconds since reset |
| `status` | LED state, uptime, button presses, lost RX bytes |

Example session:

```
ATmega328P bare-metal shell. Type 'help'.
> blink 200
ok
> status
led: blinking every 200 ms
uptime: 8421 ms
button presses: 0
rx overflows: 0
>
[button] blinking stopped
> led on
ok
```

## Project layout

```
src/main.c        timer, button, sleep, command shell
src/uart.c/.h     interrupt-driven UART0 driver
src/ringbuf.h     SPSC ring buffer used by the UART
test/             host-side unit tests
```

## Next steps

- Capture the UART frames with a logic analyzer and add the screenshots here
- Port the drivers to an STM32 (Cortex-M4) Nucleo board

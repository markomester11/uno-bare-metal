MCU         ?= atmega328p
F_CPU       ?= 16000000UL
BAUD        ?= 115200
# Old Uno clones with the old bootloader may need UPLOAD_BAUD=57600
UPLOAD_BAUD ?= 115200
PORT        ?= $(firstword $(wildcard /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/ttyACM* /dev/ttyUSB*))

# Use avr-gcc and avrdude from PATH if installed; otherwise fall back to the
# copies that Arduino IDE 2 installs on macOS.
ARDUINO_TOOLS := $(HOME)/Library/Arduino15/packages/arduino/tools

ifeq ($(shell command -v avr-gcc 2>/dev/null),)
  AVR_GCC_DIR := $(lastword $(sort $(wildcard $(ARDUINO_TOOLS)/avr-gcc/*/bin)))
  ifneq ($(AVR_GCC_DIR),)
    AVR_BIN := $(AVR_GCC_DIR)/
  endif
endif

AVRDUDE := avrdude
ifeq ($(shell command -v avrdude 2>/dev/null),)
  AVRDUDE_DIR := $(lastword $(sort $(wildcard $(ARDUINO_TOOLS)/avrdude/*)))
  ifneq ($(AVRDUDE_DIR),)
    AVRDUDE := $(AVRDUDE_DIR)/bin/avrdude -C $(AVRDUDE_DIR)/etc/avrdude.conf
  endif
endif

CC      := $(AVR_BIN)avr-gcc
OBJCOPY := $(AVR_BIN)avr-objcopy
SIZE    := $(AVR_BIN)avr-size

SRC     := src/main.c src/uart.c
HDR     := $(wildcard src/*.h)
CFLAGS  := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD)UL -Os -std=gnu11 \
           -Wall -Wextra -Werror -ffunction-sections -fdata-sections
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections

.PHONY: all flash monitor test clean

all: build/firmware.hex

build:
	mkdir -p build

build/firmware.elf: $(SRC) $(HDR) | build
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)
	$(SIZE) $@

build/firmware.hex: build/firmware.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

flash: build/firmware.hex
	@test -n "$(PORT)" || { echo "No board found. Plug in the Uno, or run: make flash PORT=/dev/cu.usbmodemXXXX"; exit 1; }
	$(AVRDUDE) -c arduino -p m328p -P $(PORT) -b $(UPLOAD_BAUD) -U flash:w:$<:i

# Exit screen with Ctrl-A, then K, then Y.
monitor:
	@test -n "$(PORT)" || { echo "No board found."; exit 1; }
	screen $(PORT) $(BAUD)

# Runs on your computer, not on the board.
test: | build
	cc -std=c11 -Wall -Wextra -Werror -Isrc test/test_ringbuf.c -o build/test_ringbuf
	./build/test_ringbuf

clean:
	rm -rf build

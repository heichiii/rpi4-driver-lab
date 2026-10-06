obj-m += rpi_gpio_led.o

KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(CURDIR)/build

.PHONY: all clean

all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" MO="$(BUILD_DIR)" modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" MO="$(BUILD_DIR)" clean

obj-m += hello.o
obj-m += rpi_gpio.o
obj-m += rpi_gpio_device.o

KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(CURDIR)/build

.PHONY: all clean

all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" MO="$(BUILD_DIR)" modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" MO="$(BUILD_DIR)" clean

obj-m += mpu6500_lab.o
mpu6500_lab-y := mpu6500_i2c.o

KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(CURDIR)/build

.PHONY: all clean

all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" MO="$(BUILD_DIR)" modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" MO="$(BUILD_DIR)" clean

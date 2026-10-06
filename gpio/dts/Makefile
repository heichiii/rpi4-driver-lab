DTC ?= dtc
BUILD_DIR := build
TARGET := $(BUILD_DIR)/rpi-gpio-led-overlay.dtbo

.PHONY: all clean

all: $(TARGET)

$(TARGET): rpi-gpio-led-overlay.dts Makefile | $(BUILD_DIR)
	$(DTC) -@ -I dts -O dtb -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -f $(TARGET)

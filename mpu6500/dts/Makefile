DTC ?= dtc
BUILD_DIR := build
TARGET := $(BUILD_DIR)/mpu6500-overlay.dtbo

.PHONY: all clean

all: $(TARGET)

$(TARGET): mpu6500-overlay.dts Makefile | $(BUILD_DIR)
	$(DTC) -@ -I dts -O dtb -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -f $(TARGET)

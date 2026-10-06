CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -D_POSIX_C_SOURCE=200809L

BUILD_DIR := build
TARGET := $(BUILD_DIR)/ledctl

.PHONY: all clean

all: $(TARGET)

$(BUILD_DIR)/%: %.c Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -f $(TARGET)

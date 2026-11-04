.DEFAULT_GOAL := all
# Shared diagnostic image for CH591 and CH592.
SUPPORTED_MCUS := CH592
ifeq ($(OS),Windows_NT)
ifneq ($(shell where riscv-none-embed-gcc 2>nul),)
PREFIX ?= riscv-none-embed
endif
endif
ifndef SINGLE_BUILD
.PHONY: all build $(SUPPORTED_MCUS)
all: $(SUPPORTED_MCUS)
$(SUPPORTED_MCUS):
	$(MAKE) SINGLE_BUILD=1 TARGET_MCU=$@ build
ifdef TARGET_MCU
build:
	$(MAKE) SINGLE_BUILD=1 TARGET_MCU=$(TARGET_MCU) build
else
build: all
endif
else
ifeq ($(filter $(TARGET_MCU),$(SUPPORTED_MCUS)),)
$(error Use TARGET_MCU=CH592; this shared firmware also supports CH591)
endif
OUTPUT_DIR := build/$(TARGET_MCU)
TARGET := $(OUTPUT_DIR)/usb_bootloader
LINKER_SCRIPT := usb_bootloader.ld
ADDITIONAL_C_FILES := ../../extralibs/fsusb.c
EXTRA_CFLAGS += -D$(TARGET_MCU)
override FILES_TO_COMPILE := ../../ch32fun/ch32fun.c usb_bootloader.c $(ADDITIONAL_C_FILES)
EXTRA_ELF_DEPENDENCIES := funconfig.h boot_config.h usb_config.h usb_bootloader.ld Makefile
include ../../ch32fun/ch32fun.mk
$(TARGET).elf: | $(OUTPUT_DIR)
$(OUTPUT_DIR):
ifeq ($(OS),Windows_NT)
	cmd /c "if not exist $(subst /,\,$@) mkdir $(subst /,\,$@)"
else
	mkdir -p "$@"
endif
endif

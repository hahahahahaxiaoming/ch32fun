.DEFAULT_GOAL := all
TARGET_MCU ?= CH570
ifneq ($(TARGET_MCU),CH570)
$(error usb_daplink supports CH570 only)
endif
ifeq ($(OS),Windows_NT)
ifneq ($(shell where riscv-none-embed-gcc 2>nul),)
PREFIX ?= riscv-none-embed
endif
endif
OUTPUT_DIR := build/CH570
TARGET := $(OUTPUT_DIR)/usb_daplink
ENABLE_FPU := 0
LINKER_SCRIPT := app.ld
override GENERATED_LD_FILE := $(OUTPUT_DIR)/generated_CH570.ld
override FILES_TO_COMPILE := ../../ch32fun/ch32fun.c ../../extralibs/fsusb.c usb_daplink.c usb_identity.c dap_validate.c vendor/CMSIS-DAP/DAP.c vendor/CMSIS-DAP/SW_DP.c
EXTRA_CFLAGS += -DCH570 -Ivendor/CMSIS-DAP
EXTRA_ELF_DEPENDENCIES := DAP_config.h cmsis_compiler.h usb_config.h usb_identity.h dap_validate.h funconfig.h vendor/CMSIS-DAP/DAP.h Makefile
LDFLAGS += -Wl,--defsym=APP_FLASH_END=0x3C000,--defsym=APP_RAM_SIZE=12288,--defsym=APP_VECTOR_TAG=0xF3F9BDA9
FLASH_COMMAND = "$(MINICHLINK)/minichlink$(if $(filter Windows_NT,$(OS)),.exe)" -C b003boot -w "$(TARGET).bin" 0x1000 -b
include ../../ch32fun/ch32fun.mk
$(GENERATED_LD_FILE) $(TARGET).elf: | $(OUTPUT_DIR)
$(OUTPUT_DIR):
ifeq ($(OS),Windows_NT)
	cmd /c "if not exist $(subst /,\,$@) mkdir $(subst /,\,$@)"
else
	mkdir -p "$@"
endif
.PHONY: all CH570 flash clean
all: build
CH570: build
flash: cv_flash
clean: cv_clean

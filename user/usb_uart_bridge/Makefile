.DEFAULT_GOAL := all
SUPPORTED_MCUS := CH570
SUPPORTED_MODES := uart rs485
ifeq ($(OS),Windows_NT)
ifneq ($(shell where riscv-none-embed-gcc 2>nul),)
PREFIX ?= riscv-none-embed
endif
endif

ifndef SINGLE_MCU_BUILD
.PHONY: all build clean flash $(SUPPORTED_MCUS) $(SUPPORTED_MODES)
all: $(SUPPORTED_MCUS)
$(SUPPORTED_MCUS):
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=$@ MODE=uart build
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=$@ MODE=rs485 build
$(SUPPORTED_MODES):
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=CH570 MODE=$@ build
ifdef TARGET_MCU
build:
ifdef MODE
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=$(TARGET_MCU) MODE=$(MODE) build
else
	$(MAKE) $(TARGET_MCU)
endif
else
build: all
endif
flash:
ifndef TARGET_MCU
	$(error Specify TARGET_MCU=CH570 and MODE=uart or rs485)
else
ifndef MODE
	$(error Specify MODE=uart or rs485)
else
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=$(TARGET_MCU) MODE=$(MODE) flash
endif
endif
clean:
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=CH570 MODE=uart clean
	$(MAKE) SINGLE_MCU_BUILD=1 TARGET_MCU=CH570 MODE=rs485 clean
else
ifeq ($(filter $(TARGET_MCU),$(SUPPORTED_MCUS)),)
$(error Unsupported TARGET_MCU '$(TARGET_MCU)')
endif
ifeq ($(filter $(MODE),$(SUPPORTED_MODES)),)
$(error Unsupported MODE '$(MODE)')
endif
OUTPUT_DIR := build/$(TARGET_MCU)/$(MODE)
TARGET := $(OUTPUT_DIR)/usb_$(MODE)
ENABLE_FPU := 0
LINKER_SCRIPT := app.ld
ADDITIONAL_C_FILES := ../../extralibs/fsusb.c uart.c usb_identity.c
override GENERATED_LD_FILE := $(OUTPUT_DIR)/generated_$(TARGET_MCU).ld
override FILES_TO_COMPILE := ../../ch32fun/ch32fun.c usb_uart_bridge.c $(ADDITIONAL_C_FILES)
EXTRA_ELF_DEPENDENCIES := uart.h ring.h board.h usb_identity.h usb_config.h funconfig.h Makefile
EXTRA_CFLAGS += -D$(TARGET_MCU) -DBRIDGE_RS485=$(if $(filter rs485,$(MODE)),1,0)
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
.PHONY: all flash clean
all: build
flash: cv_flash
clean: cv_clean
endif

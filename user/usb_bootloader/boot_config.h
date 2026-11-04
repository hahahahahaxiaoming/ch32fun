#ifndef USB_BOOT_CONFIG_H
#define USB_BOOT_CONFIG_H
#define BOOT_VERSION "1.0"
#ifndef BOOT_LOG_ENABLE
#define BOOT_LOG_ENABLE 1 /* UART1 PA9 TX, 115200 8N1; 0 removes UART/log code. */
#endif
#define BOOT_APP_ADDRESS 0x00001000u
#define BOOT_FORCE_STAY 0 /* PB4/request marker forces boot; otherwise check the app entry. */
#define BOOT_COMMON_FLASH_END 0x00030000u
#define BOOT_REQUEST_MARKER 0xB7u
#define BOOT_RUN_APP_MARKER 0xB8u
#define BOOT_LED_PIN PB23
#define BOOT_BUTTON_PIN PB4
#define BOOT_SCRATCH_DATA_SIZE 6144
#endif

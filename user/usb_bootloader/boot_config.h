#ifndef USB_BOOT_CONFIG_H
#define USB_BOOT_CONFIG_H
#define BOOT_VERSION "1.0"
#if defined(CH570)
/* CH570 must not initialize the boot UART or include boot log strings. */
#undef BOOT_LOG_ENABLE
#define BOOT_LOG_ENABLE 0
#elif !defined(BOOT_LOG_ENABLE)
#define BOOT_LOG_ENABLE 1 /* UART1 PA9 TX, 115200 8N1; 0 removes UART/log code. */
#endif
#define BOOT_APP_ADDRESS 0x00001000u
#define BOOT_FORCE_STAY 0 /* Recovery key/request marker forces boot; otherwise check the app entry. */
#if defined(CH570)
#define BOOT_COMMON_FLASH_END 0x0003C000u
#define BOOT_LED_PIN PA7
#define BOOT_BUTTON_PIN PA7
#define BOOT_APP_VECTOR_TAG 0xF3F9BDA9u
#else
#define BOOT_COMMON_FLASH_END 0x00030000u
#define BOOT_LED_PIN PB23
#define BOOT_BUTTON_PIN PB23
#define BOOT_APP_VECTOR_TAG 0xF5F9BDA9u
#endif
#define BOOT_REQUEST_MARKER 0xB7u
#define BOOT_RUN_APP_MARKER 0xB8u
#define BOOT_SCRATCH_DATA_SIZE 6144
#endif

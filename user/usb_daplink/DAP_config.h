#ifndef CH570_DAP_CONFIG_H
#define CH570_DAP_CONFIG_H
#include "ch32fun.h"
#include "cmsis_compiler.h"
#include "usb_identity.h"
#include <string.h>
#if !defined(CH570)
#error CH570 required
#endif
#define CPU_CLOCK FUNCONF_SYSTEM_CORE_CLOCK
/* Startup copies .srodata into executable RAM alongside the USB IRQ. */
#define DAP_SWD_CODE __attribute__((section(".srodata.dap_swd"), noinline, optimize("O2")))
#define DAP_IRQ_ATTRIBUTE __attribute__((interrupt))
/* Conservative software clock; actual SWCLK must be measured on hardware. */
#define IO_PORT_WRITE_CYCLES 2U
#define DELAY_SLOW_CYCLES 3U
#define DAP_SWD 1
#define DAP_JTAG 0
#define DAP_DEFAULT_PORT 1U
#define DAP_DEFAULT_SWJ_CLOCK 4000000U
#define DAP_PACKET_SIZE 64U
#define DAP_PACKET_COUNT 8U
#define SWO_UART 0
#define SWO_MANCHESTER 0
#define SWO_STREAM 0
#define TIMESTAMP_CLOCK 0U
#define DAP_UART 0
#define DAP_UART_USB_COM_PORT 0
#define TARGET_FIXED 0

__STATIC_INLINE uint8_t DAP_GetVendorString(char *s) { strcpy(s,"ch32fun"); return 8; }
__STATIC_INLINE uint8_t DAP_GetProductString(char *s) { strcpy(s,DAP_PRODUCT); return sizeof(DAP_PRODUCT); }
__STATIC_INLINE uint8_t DAP_GetSerNumString(char *s) { strcpy(s,dap_serial); return sizeof(dap_serial); }
__STATIC_INLINE uint8_t DAP_GetProductFirmwareVersionString(char *s) { strcpy(s,"1.0.0"); return 6; }
__STATIC_INLINE uint8_t DAP_GetTargetDeviceVendorString(char *s) { (void)s; return 0; }
__STATIC_INLINE uint8_t DAP_GetTargetDeviceNameString(char *s) { (void)s; return 0; }
__STATIC_INLINE uint8_t DAP_GetTargetBoardVendorString(char *s) { (void)s; return 0; }
__STATIC_INLINE uint8_t DAP_GetTargetBoardNameString(char *s) { (void)s; return 0; }
__STATIC_FORCEINLINE void PIN_SWCLK_TCK_SET(void) { funDigitalWrite(PA2,1); }
__STATIC_FORCEINLINE void PIN_SWCLK_TCK_CLR(void) { funDigitalWrite(PA2,0); }
__STATIC_FORCEINLINE uint32_t PIN_SWCLK_TCK_IN(void) { return funDigitalRead(PA2); }
__STATIC_FORCEINLINE void PIN_SWDIO_TMS_SET(void) { funDigitalWrite(PA3,1); }
__STATIC_FORCEINLINE void PIN_SWDIO_TMS_CLR(void) { funDigitalWrite(PA3,0); }
__STATIC_FORCEINLINE uint32_t PIN_SWDIO_TMS_IN(void) { return funDigitalRead(PA3); }
__STATIC_FORCEINLINE uint32_t PIN_SWDIO_IN(void) { return funDigitalRead(PA3); }
__STATIC_FORCEINLINE void PIN_SWDIO_OUT(uint32_t bit) { funDigitalWrite(PA3,bit & 1); }
__STATIC_FORCEINLINE void PIN_SWDIO_OUT_ENABLE(void) { R32_PA_DIR |= 1U << 3; }
__STATIC_FORCEINLINE void PIN_SWDIO_OUT_DISABLE(void) { R32_PA_DIR &= ~(1U << 3); }
__STATIC_INLINE void PORT_SWD_SETUP(void) {
    funDigitalWrite(PA2,1); funDigitalWrite(PA3,1);
    funPinMode(PA2,GPIO_ModeOut_PP_5mA); funPinMode(PA3,GPIO_ModeOut_PP_5mA);
}
__STATIC_INLINE void PORT_OFF(void) {
    funPinMode(PA2,GPIO_ModeIN_Floating); funPinMode(PA3,GPIO_ModeIN_Floating);
}
__STATIC_INLINE void DAP_SETUP(void) { PORT_OFF(); }
__STATIC_INLINE uint32_t PIN_TDI_IN(void) { return 0; }
__STATIC_INLINE uint32_t PIN_TDO_IN(void) { return 0; }
__STATIC_INLINE uint32_t PIN_nTRST_IN(void) { return 1; }
__STATIC_INLINE uint32_t PIN_nRESET_IN(void) { return 1; }
__STATIC_INLINE void PIN_TDI_OUT(uint32_t b) { (void)b; }
__STATIC_INLINE void PIN_nTRST_OUT(uint32_t b) { (void)b; }
__STATIC_INLINE void PIN_nRESET_OUT(uint32_t b) { (void)b; }
__STATIC_INLINE void LED_CONNECTED_OUT(uint32_t b) { (void)b; }
__STATIC_INLINE void LED_RUNNING_OUT(uint32_t b) { (void)b; }
__STATIC_INLINE uint32_t TIMESTAMP_GET(void) { return 0; }
__STATIC_INLINE uint8_t RESET_TARGET(void) { return 0; }
#endif

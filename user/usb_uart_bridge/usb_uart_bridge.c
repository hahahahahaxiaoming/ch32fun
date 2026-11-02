#include "ch32fun.h"
#include "fsusb.h"
#include "uart.h"
#include "usb_identity.h"
#include <string.h>

static uint8_t line_coding[7] = {0x00, 0xc2, 0x01, 0, 0, 0, 8};

int HandleInRequest(struct _USBState *ctx, int endp, uint8_t *data, int len) {
    bridge_capture_rx();
    if (endp == 3) return bridge_usb_in_complete(data);
    return 0;
}
void HandleDataOut(struct _USBState *ctx, int endp, uint8_t *data, int len) {
    bridge_capture_rx();
    if (endp == 2) {
        /* fsusb toggles RX before the callback, even for duplicate packets. */
        if (!(USBFS->INT_ST & RB_UIS_TOG_OK)) {
            UEP_CTRL_TX(2) ^= USBFS_UEP_R_TOG;
            return;
        }
        bridge_usb_out(data, len);
    } else if (endp == 0 && ctx->USBFS_SetupReqCode == CDC_SET_LINE_CODING) {
        ctx->USBFS_SetupReqLen = 0;
        if (len == sizeof(line_coding) && bridge_set_line_coding(data))
            memcpy(line_coding, data, sizeof(line_coding));
    }
}
int HandleSetupCustom(struct _USBState *ctx, int setup_code) {
    bridge_capture_rx();
    if ((ctx->USBFS_SetupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_CLASS) return 0;
    switch (setup_code) {
    case CDC_GET_LINE_CODING:
        ctx->pCtrlPayloadPtr = line_coding;
        return ctx->USBFS_SetupReqLen < 7 ? ctx->USBFS_SetupReqLen : 7;
    case CDC_SET_LINE_CODING:
        return ctx->USBFS_SetupReqLen == 7 ? 7 : 0;
    case CDC_SET_LINE_CTLSTE:
        return -1;
    default:
        return 0;
    }
}
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void) {
    SysTick->CMP = SysTick->CNT + FUNCONF_SYSTEM_CORE_CLOCK / 1000;
    SysTick->SR = 0;
    bridge_tick();
}
int main(void) {
    SystemInit();
    usb_identity_init();
    /* Shared queue service runs at one preemption priority; handlers serialize. */
    NVIC_SetPriority(USB_IRQn, 0);
    NVIC_SetPriority(BRIDGE_UART_IRQ, 0);
    NVIC_SetPriority(SysTick_IRQn, 0);
    USBFSSetup();
    bridge_init();
    SysTick->CTLR = 0;
    SysTick->CNT = 0;
    SysTick->CMP = FUNCONF_SYSTEM_CORE_CLOCK / 1000 - 1;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
    NVIC_EnableIRQ(SysTick_IRQn);
    while (1) __asm__ volatile("wfi");
}

#include "ch32fun.h"
#include "fsusb.h"
#include "boot_config.h"
#include <string.h>

#define SCRATCHPAD_SIZE (BOOT_SCRATCH_DATA_SIZE + 128)
/* Preserve the b003 ABI: boot_usercode pointer immediately before scratchpad. */
__attribute__((section(".boot_address"))) volatile uint32_t boot_usercode_address;
__attribute__((section(".scratchpad"))) uint8_t scratchpad[SCRATCHPAD_SIZE];
__attribute__((section(".runwordpad"))) volatile int32_t runwordpad;
static volatile uint32_t cmd_len;

#if BOOT_LOG_ENABLE
static void uart_puts(const char *text) {
    while(*text) {
        while(!(R8_UART1_LSR & RB_LSR_TX_ALL_EMP)) { }
        R8_UART1_THR = *text++;
    }
}

static void boot_print_version(uint8_t reset_flag, int run_app) {
    funDigitalWrite(PA9, 1);
    funPinMode(PA9, GPIO_ModeOut_PP_5mA);
    R16_UART1_DL = ((10 * FUNCONF_SYSTEM_CORE_CLOCK / 8 / 115200) + 5) / 10;
    R8_UART1_FCR = RB_FCR_TX_FIFO_CLR | RB_FCR_RX_FIFO_CLR | RB_FCR_FIFO_EN;
    R8_UART1_LCR = RB_LCR_WORD_SZ;
    R8_UART1_DIV = 1;
    R8_UART1_IER = RB_IER_TXD_EN;
    /* The reset cause is a three-bit field, so its decimal digit is always 0..7. */
    static char banner[] = "[BOOT] V" BOOT_VERSION " RST=0";
    banner[sizeof(banner) - 2] = '0' + reset_flag;
    uart_puts(banner);
    uart_puts(run_app ? " -> APP\r\n" : " -> BOOT\r\n");
    /* Drain UART before jumping or resetting USB. No UART interrupts enabled. */
    while(!(R8_UART1_LSR & RB_LSR_TX_ALL_EMP)) { }
    R8_UART1_IER = 0;
}
#endif

static int app_present(void) {
    uint32_t first = *(volatile uint32_t *)BOOT_APP_ADDRESS;
    /* CH592F_DEMO startup_CH592.S embeds this word in the vector table. */
    uint32_t vector_tag = *(volatile const uint32_t *)(BOOT_APP_ADDRESS + 0x14u);
    return vector_tag == 0xf5f9bda9u &&
           ((first & 0x7f) == 0x6f || (first & 0xe003) == 0xa001);
}

static void start_app(void) __attribute__((noreturn, noinline));
static void start_app(void) {
    /* The SDK startup overwrites the RAM vector table. Prevent interrupts
     * throughout this handoff; the SDK startup restores mstatus itself. */
    __asm__ volatile(ADD_ARCH_ZICSR "csrci mstatus, 8" ::: "memory");
    /* Normally USB has not been initialized on this reset path. Still leave
     * its interrupt source and PFIC state idle before the app replaces mtvec. */
    NVIC_DisableIRQ(USB_IRQn);
    USBFS->INT_EN = 0;
    USBFS->INT_FG = 0xff;
    NVIC_ClearPendingIRQ(USB_IRQn);
    /* ch32fun startup already enabled the free-running SysTick. */
    NVIC_DisableIRQ(SysTick_IRQn);
    SysTick->CTLR = 0;
    SysTick->CNT = 0;
    SysTick->CMP = 0;
    SysTick->SR = 0;
    NVIC_ClearPendingIRQ(SysTick_IRQn);
    __asm__ volatile("fence rw, rw\n fence.i\n li t0, %0\n jr t0\n" :: "i"(BOOT_APP_ADDRESS) : "t0", "memory");
    __builtin_unreachable();
}

/* Called by minichlink -b through the pointer at scratchpad[-4]. Always cold
 * restart; do not jump from a USB ISR/RAM stub into the SDK application. */
void boot_usercode(void) __attribute__((noreturn));
void boot_usercode(void) {
    /* Allow the final SET_REPORT status transaction to finish before detach. */
    Delay_Ms(50);
    USBFSReset();
    USBFS->INT_EN = 0;
    USBFS->INT_FG = 0xff;
    NVIC_ClearPendingIRQ(USB_IRQn);
    /* Match the official IAP's detach interval before leaving USB mode. */
    Delay_Ms(10);
    R8_GLOB_RESET_KEEP = BOOT_RUN_APP_MARKER;
    SYS_SAFE_ACCESS(R8_RST_WDOG_CTRL |= 1;);
    while(1) { }
}

void USBFS_InternalFinishSetup(void) {
    USBFS->UEP4_1_MOD = 0;
    UEP_CTRL_LEN(0) = UEP_CTRL_LEN(1) = 0;
    memset(USBFSCTX.ep_buffers[0], 0, 64);
    memset(USBFSCTX.ep_buffers[1], 0, 64);
    USBFSCTX.endpoints[0].mode = USBFS_EP_MODE_RX | USBFS_EP_MODE_TX;
    USBFSCTX.endpoints[1].mode = USBFS_EP_MODE_TX;
    USBFSCTX.endpoints[0].busy = USBFSCTX.endpoints[1].busy = 0;
    USBFS->UEP4_1_MOD = FUSB_EP1_MODE << 4;
    UEP_DMA(0) = (uintptr_t)USBFSCTX.ep_buffers[0];
    UEP_DMA(1) = (uintptr_t)USBFSCTX.ep_buffers[1];
    USBFSCTX.endpoints[0].in = USBFSCTX.endpoints[0].out = USBFSCTX.ep_buffers[0];
    USBFSCTX.endpoints[1].in = USBFSCTX.ep_buffers[1];
    UEP_CTRL_TX(0) = USBFS_UEP_T_RES_NAK | USBFS_UEP_R_RES_ACK | CHECK_USBFS_UEP_T_AUTO_TOG;
    UEP_CTRL_TX(1) = USBFS_UEP_T_RES_NAK;
}

__USBFS_FUN_ATTRIBUTE
static void process_loop(void) {
    int32_t countdown = 0;
    uint32_t last_command = funSysTick32();
    while(1) {
        /* Feed an already-running IWDG, but never enable it here. */
        R32_IWDG_KR = 0xaaaa;
        int32_t command = runwordpad;
        if(command) {
            last_command = funSysTick32();
            countdown = command > 0 ? command - 1 : 0;
            runwordpad = 0;
        } else if(countdown > 0 && --countdown == 0) {
            typedef void (*stub_t)(uint32_t *, volatile int32_t *);
            ((stub_t)(scratchpad + 4))((uint32_t *)scratchpad, &runwordpad);
            last_command = funSysTick32();
        } else if(!BOOT_FORCE_STAY && !countdown &&
                  (uint32_t)(funSysTick32() - last_command) >= Ticks_from_Ms(30000u)) {
            NVIC_DisableIRQ(USB_IRQn);
            if(!runwordpad && app_present()) boot_usercode();
            NVIC_EnableIRQ(USB_IRQn);
            last_command = funSysTick32();
        }
    }
}

int main(void) {
#if BOOT_LOG_ENABLE
    /* Snapshot the reason before clock/peripheral initialization. */
    uint8_t reset_flag = R8_RESET_STATUS & RB_RESET_FLAG;
#endif
    /* Recovery key is handled here, independently of the application. */
    funPinMode(BOOT_BUTTON_PIN, GPIO_ModeIN_PU);
    for(volatile int settle = 0; settle < 2000; settle++) __asm__ volatile("nop");
    int force_boot = !funDigitalRead(BOOT_BUTTON_PIN);
    uint8_t marker = R8_GLOB_RESET_KEEP;
    if(marker == BOOT_REQUEST_MARKER || marker == BOOT_RUN_APP_MARKER)
        R8_GLOB_RESET_KEEP = 0;
    int run_app = !BOOT_FORCE_STAY && marker != BOOT_REQUEST_MARKER &&
                  (!force_boot || marker == BOOT_RUN_APP_MARKER) && app_present();
#if BOOT_LOG_ENABLE
    SystemInit();
    boot_print_version(reset_flag, run_app);
#endif
    if(run_app) start_app();
    funDigitalWrite(BOOT_LED_PIN, 1);
    funPinMode(BOOT_LED_PIN, GPIO_ModeOut_PP_5mA);
#if !BOOT_LOG_ENABLE
    SystemInit();
#endif
    boot_usercode_address = (uint32_t)boot_usercode;
    runwordpad = 0;
    /* Force a detectable detach after a factory-ISP software launch as well. */
    USBFSReset();
    Delay_Ms(100);
    USBFSSetup();
    process_loop();
}

void HandleHidUserReportDataOut(struct _USBState *ctx, uint8_t *data, int len) { }
int HandleHidUserReportDataIn(struct _USBState *ctx, uint8_t *data, int len) { return len; }
int HandleHidUserGetReportSetup(struct _USBState *ctx, tusb_control_request_t *req) {
    if(req->wLength > SCRATCHPAD_SIZE) req->wLength = SCRATCHPAD_SIZE;
    ctx->pCtrlPayloadPtr = scratchpad;
    return req->wLength;
}
int HandleHidUserSetReportSetup(struct _USBState *ctx, tusb_control_request_t *req) {
    if(req->wLength > SCRATCHPAD_SIZE) req->wLength = SCRATCHPAD_SIZE;
    runwordpad = 1;
    ctx->pCtrlPayloadPtr = scratchpad;
    cmd_len = req->wLength;
    return req->wLength;
}
void HandleHidUserReportOutComplete(struct _USBState *ctx) {
    if(cmd_len < 8 || cmd_len > SCRATCHPAD_SIZE) return;
    uint32_t trailer;
    memcpy(&trailer, scratchpad + cmd_len - 4, 4);
    if(trailer == 0x1234abcd) {
        memset(scratchpad + cmd_len - 4, 0, 4);
        runwordpad = 100;
        ctx->pCtrlPayloadPtr = 0;
    }
}

#include "fsusb.h"
#include "DAP_config.h"
#include "DAP.h"
#include "dap_validate.h"

static const uint8_t ms_compat[] = {
    40,0,0,0, 0,1, 4,0, 1,0,0,0,0,0,0,0,
    0,1,'W','I','N','U','S','B',0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0
};
static const struct __attribute__((packed)) {
    uint32_t length;
    uint16_t version,index,count;
    uint32_t section_length,type;
    uint16_t name_length,name[21];
    uint32_t data_length;
    uint16_t guid[40];
} ms_properties = {
    146,0x100,5,1,136,7,42,u"DeviceInterfaceGUIDs",80,
    u"{CDB3B5AD-293B-4663-AA36-1AAE46463776}\0"
};
_Static_assert(sizeof(ms_properties) == 146, "WCID properties size");

/* A slot remains occupied until its response has actually reached the host. */
/* Unadvertised spare capacity keeps OUT receptive to asynchronous Abort
 * when a conforming host has all advertised packets outstanding. */
#define SLOT_COUNT (2 * DAP_PACKET_COUNT)
static uint8_t requests[SLOT_COUNT][DAP_PACKET_SIZE];
static uint8_t responses[SLOT_COUNT][DAP_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t request_len[SLOT_COUNT], response_len[SLOT_COUNT];
static volatile uint32_t received, processed, sent, epoch;
static volatile uint8_t tx_active, reset_pending;
static volatile uint32_t milliseconds, last_activity;
static volatile uint8_t activity_seen;

static void activity(void) { last_activity = milliseconds; activity_seen = 1; }
static void rx_enable(void) {
    UEP_CTRL_RX(2) = (UEP_CTRL_RX(2) & ~USBFS_UEP_R_RES_MASK) |
        ((received - sent < SLOT_COUNT) ? USBFS_UEP_R_RES_ACK : USBFS_UEP_R_RES_NAK);
}
void dap_usb_reset(void) {
    DAP_TransferAbort = 1;
    ++epoch;
    received = processed = sent = 0;
    tx_active = 0;
    reset_pending = 1;
    PORT_OFF();
}
int HandleSetupCustom(struct _USBState *ctx, int code) {
    const uint8_t *p;
    unsigned length;
    uint16_t index = ctx->USBFS_IndexValue >> 16;
    uint16_t value = ctx->USBFS_IndexValue;
    if (code != DAP_MS_VENDOR_CODE || value != 0) return 0;
    if (ctx->USBFS_SetupReqType == 0xc0 && index == 4) {
        p = ms_compat; length = sizeof(ms_compat);
    } else if ((ctx->USBFS_SetupReqType == 0xc1 || ctx->USBFS_SetupReqType == 0xc0) && index == 5) {
        p = (const uint8_t *)&ms_properties; length = sizeof(ms_properties);
    } else return 0;
    ctx->pCtrlPayloadPtr = (uint8_t *)p;
    return ctx->USBFS_SetupReqLen < length ? ctx->USBFS_SetupReqLen : length;
}
void HandleDataOut(struct _USBState *ctx, int ep, uint8_t *data, int len) {
    (void)ctx;
    if (ep != 2) return;
    if (!(USBFS->INT_ST & RB_UIS_TOG_OK)) {
        UEP_CTRL_RX(2) ^= USBFS_UEP_R_TOG;
        return;
    }
    if (len <= 0 || (unsigned)len > DAP_PACKET_SIZE) return;
    activity();
    /* Abort is asynchronous and has no response packet. */
    if (data[0] == ID_DAP_TransferAbort) { DAP_TransferAbort = 1; return; }
    if (received - sent >= SLOT_COUNT) return;
    unsigned slot = received % SLOT_COUNT;
    memcpy(requests[slot],data,len);
    request_len[slot] = len;
    ++received;
    rx_enable();
}
int HandleInRequest(struct _USBState *ctx, int ep, uint8_t *data, int len) {
    (void)ctx; (void)data; (void)len;
    if (ep == 1 && tx_active) {
        ++sent; tx_active = 0; activity(); rx_enable();
        /* fsusb toggles DATA0/1 and arms the returned length after this callback.
         * Keep the next completed response flowing even while main does SWD.
         * The slot stays immutable until its IN completion releases it. */
        if (ctx->USBFS_DevConfig && sent != processed &&
            !ctx->USBFS_errata_dont_send_endpoint_in_window && !ctx->USBFS_SetupReqLen) {
            unsigned slot = sent % SLOT_COUNT;
            UEP_DMA(1) = (uintptr_t)responses[slot];
            tx_active = 1;
            return response_len[slot];
        }
    }
    return 0;
}
void SysTick_Handler(void) DAP_IRQ_ATTRIBUTE;
void SysTick_Handler(void) {
    SysTick->CMP = SysTick->CNT + CPU_CLOCK / 1000;
    SysTick->SR = 0;
    ++milliseconds;
    /* 50 ms on/off while traffic continues, off 120 ms after last packet. */
    funDigitalWrite(PA7,activity_seen && milliseconds - last_activity < 120 &&
                    (milliseconds / 50) % 2 == 0);
}
int main(void) {
    SystemInit();
    R16_PIN_ALTERNATE &= ~RB_PIN_DEBUG_EN;
    usb_identity_init(); DAP_Setup();
    funDigitalWrite(PA7,0); funPinMode(PA7,GPIO_ModeOut_PP_5mA);
    SysTick->CTLR = 0; SysTick->CNT = 0; SysTick->CMP = CPU_CLOCK / 1000 - 1;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
    NVIC_EnableIRQ(SysTick_IRQn);
    USBFSSetup();
    for (;;) {
        uint8_t request[64], response[64];
        unsigned length = 0;
        NVIC_DisableIRQ(USB_IRQn);
        if (reset_pending) { DAP_Setup(); reset_pending = 0; }
        uint32_t generation = epoch, current = processed;
        int ready = USBFSCTX.USBFS_DevConfig && current != received;
        unsigned slot = current % SLOT_COUNT;
        if (ready) {
            length = request_len[slot];
            memcpy(request,requests[slot],length);
        }
        NVIC_EnableIRQ(USB_IRQn);
        if (ready) {
            /* Local copies survive a USB reset/reuse of the ring during SWD. */
            unsigned n = 1;
            response[0] = ID_DAP_Invalid;
            if (dap_validate(request,length)) n = DAP_ExecuteCommand(request,response) & 0xffff;
            NVIC_DisableIRQ(USB_IRQn);
            if (epoch == generation) {
                memcpy(responses[slot],response,n);
                response_len[slot] = n;
                ++processed;
            } else PORT_OFF();
            NVIC_EnableIRQ(USB_IRQn);
        }
        NVIC_DisableIRQ(USB_IRQn);
        if (USBFSCTX.USBFS_DevConfig && !tx_active && sent != processed) {
            unsigned slot = sent % SLOT_COUNT;
            if (USBFS_SendEndpointNEW(1,responses[slot],response_len[slot],0) == 0) tx_active = 1;
        }
        NVIC_EnableIRQ(USB_IRQn);
    }
}

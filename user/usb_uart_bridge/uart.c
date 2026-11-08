#include "uart.h"
#include "fsusb.h"
#include <string.h>

static uint8_t tx_data[UART_TX_BUF_SIZE];
static uint8_t rx_data[UART_RX_BUF_SIZE];
static ByteRing tx = {tx_data, sizeof(tx_data), 0, 0};
static ByteRing rx = {rx_data, sizeof(rx_data), 0, 0};
BridgeStats bridge_stats;
static volatile uint8_t in_active, need_zlp, out_paused;
static volatile uint32_t rx_idle_ms;
static uint8_t ready;
volatile uint32_t bridge_ticks_per_ms = FUNCONF_SYSTEM_CORE_CLOCK / 1000;

typedef struct {
    uint32_t hz;
    SYS_CLKTypeDef source;
} BridgeClock;

static const BridgeClock baud_clocks[] = {
    {FUNCONF_SYSTEM_CORE_CLOCK, CLK_SOURCE_CH5XX},
    {100000000, CLK_SOURCE_PLL_100MHz},
    {75000000, CLK_SOURCE_PLL_75MHz},
    {50000000, CLK_SOURCE_PLL_50MHz},
    {40000000, CLK_SOURCE_PLL_40MHz},
    {30000000, CLK_SOURCE_PLL_30MHz},
    {25000000, CLK_SOURCE_PLL_25MHz},
    {24000000, CLK_SOURCE_PLL_24MHz},
    {20000000, CLK_SOURCE_PLL_20MHz},
    {16000000, CLK_SOURCE_HSE_16MHz},
};

static struct {
    const BridgeClock *clock;
    uint16_t divisor;
    uint8_t lcr;
#if BRIDGE_RS485
    uint8_t frame_bits;
#endif
} pending_line;

#if BRIDGE_RS485
static uint8_t rs485_active;
static uint32_t rs485_frame_ticks;
static uint32_t rs485_bit_ticks;
static void rs485_timer_stop(void) {
    BRIDGE_TIMER->CTRL_MOD = 0;
    BRIDGE_TIMER->INTER_EN = 0;
    BRIDGE_TIMER->INT_FLAG = RB_TMR_IF_CYC_END;
}
static void rs485_timer_arm(uint32_t ticks) {
    BRIDGE_TIMER->CTRL_MOD = RB_TMR_ALL_CLEAR;
    BRIDGE_TIMER->CNT_END = ticks;
    BRIDGE_TIMER->INT_FLAG = RB_TMR_IF_CYC_END;
    BRIDGE_TIMER->INTER_EN = RB_TMR_IE_CYC_END;
    BRIDGE_TIMER->CTRL_MOD = RB_TMR_COUNT_EN;
}
static void rs485_begin(void) {
    rs485_timer_stop();
    if (!rs485_active) {
        funDigitalWrite(RS485_DIR_PIN, 1);
        rs485_active = 1;
        DelaySysTick(bridge_ticks_per_ms / 1000);
    }
}
/* TFC can be zero while the shift register is sending the final stop bit. */
void TMR1_IRQHandler(void) __attribute__((interrupt));
void TMR1_IRQHandler(void) {
    rs485_timer_stop();
    bridge_capture_rx();
    if (!rs485_active) return;
    if (!ring_count(&tx) && (BRIDGE_UART->LSR & RB_LSR_TX_ALL_EMP)) {
        funDigitalWrite(RS485_DIR_PIN, 0);
        rs485_active = 0;
    } else rs485_timer_arm(BRIDGE_UART->TFC ? rs485_frame_ticks : rs485_bit_ticks);
}
#endif

/* These routines run only in non-nesting interrupt handlers after startup.
 * Each queue has one producer and one consumer; no shared read/modify count. */
static void tx_fill(void) __attribute__((section(".srodata.bridge_tx"), noinline));
static void tx_fill(void) {
    uint8_t byte;
    if (pending_line.divisor) return;
#if BRIDGE_RS485
    if (ring_count(&tx)) rs485_begin();
#endif
    while (BRIDGE_UART->TFC < UART_FIFO_SIZE && ring_pop(&tx, &byte)) {
        BRIDGE_UART->THR_RBR = byte;
        bridge_stats.tx_bytes++;
    }
    if (ring_count(&tx)) BRIDGE_UART->IER |= RB_IER_THR_EMPTY;
    else BRIDGE_UART->IER &= ~RB_IER_THR_EMPTY;
#if BRIDGE_RS485
    if (!ring_count(&tx) && rs485_active)
        rs485_timer_arm(rs485_frame_ticks);
#endif
}

static void out_resume(void) {
    if (out_paused && !pending_line.divisor && ring_free(&tx) >= USBFS_PACKET_SIZE) {
        out_paused = 0;
        USBFS_SendACK(2, 0);
    }
}

/* Return a packet length, -1 for a ZLP, 0 if nothing is ready. */
static int next_in(uint8_t *data) __attribute__((section(".srodata.bridge_in"), noinline));
static int next_in(uint8_t *data) {
    bridge_capture_rx();
    uint32_t count = ring_count(&rx);
    if (count && (count >= USBFS_PACKET_SIZE || rx_idle_ms >= 1)) {
        unsigned len = count < USBFS_PACKET_SIZE ? count : USBFS_PACKET_SIZE;
        uint32_t read = rx.read;
        for (unsigned i = 0; i < len; i++) {
            data[i] = rx.data[(read + i) & (UART_RX_BUF_SIZE - 1)];
            /* Same-priority IRQs cannot preempt this copy. Service the UART
             * during USB packet preparation instead of waiting for IRQ exit. */
            if ((i & 15) == 15) bridge_capture_rx();
        }
        __asm__ volatile("" ::: "memory");
        rx.read = read + len;
        need_zlp = (len == USBFS_PACKET_SIZE);
        return len;
    }
    if (!count && need_zlp) {
        need_zlp = 0;
        return -1;
    }
    return 0;
}

static void usb_kick(void) {
    if (!ready || !USBFSCTX.USBFS_DevConfig || in_active ||
        USBFSCTX.endpoints[3].busy || USBFSCTX.USBFS_SetupReqLen ||
        USBFSCTX.USBFS_errata_dont_send_endpoint_in_window || (USBFS->INT_ST & 0x80)) return;
    /* No interrupt nesting: checks and queue consumption are atomic here. */
    uint8_t *data = USBFSCTX.endpoints[3].in;
    int len = next_in(data);
    if (len) {
        UEP_DMA(3) = (uintptr_t)data;
        UEP_CTRL_LEN(3) = len < 0 ? 0 : len;
        USBFSCTX.endpoints[3].busy = 1;
        in_active = 1;
        USBFS_SendACK(3, 1);
    }
}

int bridge_usb_in_complete(uint8_t *data) {
    /* fsusb consumes the return value AFTER this callback, updating LEN/ACK.
     * Do not call SendEndpoint here: its state would be overwritten by fsusb. */
    int len = next_in(data);
    in_active = (len != 0);
    return len;
}

void bridge_usb_out(const uint8_t *data, unsigned len) {
    bridge_capture_rx();
    if (len > USBFS_PACKET_SIZE || ring_free(&tx) < len) {
        bridge_stats.tx_ring_overflow++;
        USBFS_SendNAK(2, 0);
        out_paused = 1;
        return;
    }
    for (unsigned i = 0; i < len; i++) {
        ring_push(&tx, data[i]);
        if ((i & 15) == 15) bridge_capture_rx();
    }
    /* Reserve a whole maximum packet before accepting another OUT. */
    if (ring_free(&tx) < USBFS_PACKET_SIZE) {
        out_paused = 1;
        USBFS_SendNAK(2, 0);
    }
    tx_fill();
    out_resume();
}

static const BridgeClock *select_baud_clock(uint32_t baud, uint16_t *divisor) {
    if (baud == 512000) {
        *divisor = 18;
        return &baud_clocks[2]; // 75 MHz, actual baud 520833 (+1.725%).
    }
    if (baud == 2000000) {
        *divisor = 6;
        return &baud_clocks[1]; // 100 MHz, actual baud 2083333 (+4.167%).
    }
    if (baud == 2500000) {
        *divisor = 5;
        return &baud_clocks[1]; // 100 MHz, exact 2500000 baud.
    }
    const BridgeClock *best = NULL;
    uint32_t best_error = 0;
    *divisor = 0;
    for (unsigned i = 0; i < sizeof(baud_clocks) / sizeof(baud_clocks[0]); i++) {
        const BridgeClock *clock = &baud_clocks[i];
        if (clock->source == CLK_SOURCE_HSE_16MHz && baud != 2000000) continue;
        uint32_t base = clock->hz / 8;
        uint32_t rounded = (base + baud / 2) / baud;
        if (!rounded || rounded > UINT16_MAX) continue;
        uint32_t floor = base / baud;
        /* Compare both adjacent divisors. The absolute baud error is error / dl. */
        for (uint32_t dl = floor ? floor : 1; dl <= floor + 1 && dl <= UINT16_MAX; dl++) {
            uint32_t target = baud * dl;
            uint32_t error = base > target ? base - target : target - base;
            if (!best || (uint64_t)error * *divisor < (uint64_t)best_error * dl) {
                best = clock;
                *divisor = dl;
                best_error = error;
            }
        }
        if (best && !best_error) break;
    }
    return best;
}

/* The PLL stays powered for USB even when the CPU switches to HSE. */
static void switch_core_clock(SYS_CLKTypeDef source) __attribute__((section(".highcode"), noinline));
static void switch_core_clock(SYS_CLKTypeDef source) {
    if (source == CLK_SOURCE_HSE_16MHz) {
        SYS_SAFE_ACCESS(
            R8_CLK_SYS_CFG = source;
            R8_FLASH_CFG = 0x02;
        );
    } else {
        SYS_SAFE_ACCESS(
            R8_FLASH_CFG = 0x01;
            R8_FLASH_SCK |= 1 << 4;
            R8_CLK_SYS_CFG = source;
        );
    }
}

static void apply_pending_line(void) {
    uint16_t div = pending_line.divisor;
    if (!div || (ready && !(BRIDGE_UART->LSR & RB_LSR_TX_ALL_EMP))) return;

#if BRIDGE_RS485
    rs485_timer_stop();
    if (rs485_active) {
        funDigitalWrite(RS485_DIR_PIN, 0);
        rs485_active = 0;
    }
#endif
    BRIDGE_UART->IER = 0;
    const BridgeClock *clock = pending_line.clock;
    if (R8_CLK_SYS_CFG != clock->source) {
        switch_core_clock(clock->source);
        bridge_ticks_per_ms = clock->hz / 1000;
        SysTick->CMP = SysTick->CNT + bridge_ticks_per_ms;
        SysTick->SR = 0;
    }
    /* Two-byte RX trigger amortizes IRQ entry while leaving six FIFO slots free. */
    BRIDGE_UART->FCR = RB_FCR_FIFO_EN | RB_FCR_RX_FIFO_CLR | RB_FCR_TX_FIFO_CLR | 0x40;
    BRIDGE_UART->LCR = pending_line.lcr;
    BRIDGE_UART->DIV = 1;
    BRIDGE_UART->DL = div;
    BRIDGE_UART->MCR = RB_MCR_INT_OE;
    rx.read = rx.write = 0;
    rx_idle_ms = 0;
    need_zlp = 0;
    bridge_stats.actual_baud = clock->hz / 8 / div;
#if BRIDGE_RS485
    rs485_bit_ticks = div * 8;
    rs485_frame_ticks = rs485_bit_ticks * pending_line.frame_bits;
#endif
    pending_line.divisor = 0;
    BRIDGE_UART->IER = RB_IER_TXD_EN | RB_IER_RECV_RDY | RB_IER_LINE_STAT;
    if (ready && USBFSCTX.USBFS_DevConfig) {
        if (ring_count(&tx)) tx_fill();
        out_resume();
    }
}

int bridge_set_line_coding(const uint8_t *coding) {
    uint32_t baud = (uint32_t)coding[0] | ((uint32_t)coding[1] << 8) |
                    ((uint32_t)coding[2] << 16) | ((uint32_t)coding[3] << 24);
    if (!baud || coding[4] > 2 || coding[4] == 1 || coding[5] > 4 ||
        coding[6] < 5 || coding[6] > 8) return 0;
    uint16_t div;
    const BridgeClock *clock = select_baud_clock(baud, &div);
    if (!clock) return 0;

    static const uint8_t parity[] = {0, 0x08, 0x18, 0x28, 0x38};
    pending_line.clock = clock;
    pending_line.divisor = div;
    pending_line.lcr = (coding[6] - 5) | (coding[4] == 2 ? RB_LCR_STOP_BIT : 0) | parity[coding[5]];
#if BRIDGE_RS485
    pending_line.frame_bits = 1 + coding[6] + (coding[5] != 0) + (coding[4] == 2 ? 2 : 1);
#endif
    /* Drop the old software TX queue, but let bytes already in UART finish. */
    tx.read = tx.write;
    BRIDGE_UART->IER &= ~RB_IER_THR_EMPTY;
    if (ready && USBFSCTX.USBFS_DevConfig) {
        USBFS_SendNAK(2, 0);
        out_paused = 1;
    }
    apply_pending_line();
    return 1;
}

void bridge_tick(void) {
    bridge_capture_rx();
    apply_pending_line();
    if (rx_idle_ms < 1000) rx_idle_ms++;
    if (ready && !USBFSCTX.USBFS_DevConfig) {
        in_active = need_zlp = 0;
        tx.read = tx.write;
        rx.read = rx.write;
        out_paused = 0;
        BRIDGE_UART->IER &= ~RB_IER_THR_EMPTY;
        return;
    }
    out_resume();
    usb_kick();
}

void bridge_capture_rx(void) __attribute__((section(".srodata.bridge_rx"), noinline));
void bridge_capture_rx(void) {
    uint8_t status = BRIDGE_UART->LSR;
    if (status & RB_LSR_OVER_ERR) bridge_stats.rx_overrun++;
    if (status & RB_LSR_FRAME_ERR) bridge_stats.rx_frame_error++;
    if (status & RB_LSR_PAR_ERR) bridge_stats.rx_parity_error++;
    uint32_t write = rx.write;
    uint32_t free = UART_RX_BUF_SIZE - (write - rx.read);
    uint32_t count = 0;
    uint32_t dropped = 0;
    while (BRIDGE_UART->RFC) {
        uint8_t byte = BRIDGE_UART->THR_RBR;
        count++;
        if (free) {
            rx.data[write & (UART_RX_BUF_SIZE - 1)] = byte;
            write++;
            free--;
        } else dropped++;
    }
    if (count) {
        __asm__ volatile("" ::: "memory");
        rx.write = write;
        bridge_stats.rx_bytes += count;
        bridge_stats.rx_ring_overflow += dropped;
        rx_idle_ms = 0;
    }
}

void BRIDGE_UART_HANDLER(void) __attribute__((interrupt, section(".srodata.bridge_irq")));
void BRIDGE_UART_HANDLER(void) {
    /* Reading IIR acknowledges THR-empty. Keep USB work out of this ISR. */
    (void)BRIDGE_UART->IIR;
    bridge_capture_rx();
    if (BRIDGE_UART->IER & RB_IER_THR_EMPTY) tx_fill();
}

void bridge_init(void) {
    funDigitalWrite(UART_TX_PIN, 1);
    funPinMode(UART_RX_PIN, GPIO_ModeIN_PU);
    funPinMode(UART_TX_PIN, GPIO_ModeOut_PP_5mA);
#if BRIDGE_RS485
    funDigitalWrite(RS485_DIR_PIN, 0);
    funPinMode(RS485_DIR_PIN, GPIO_ModeOut_PP_5mA);
    rs485_timer_stop();
    NVIC_SetPriority(TMR1_IRQn, 0);
    NVIC_EnableIRQ(TMR1_IRQn);
#endif
    const uint8_t coding[7] = {0x00, 0xc2, 0x01, 0, 0, 0, 8};
    bridge_set_line_coding(coding);
    ready = 1;
    NVIC_EnableIRQ(BRIDGE_UART_IRQ);
}

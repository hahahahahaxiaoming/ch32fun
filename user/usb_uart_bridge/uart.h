#ifndef USB_UART_BRIDGE_UART_H
#define USB_UART_BRIDGE_UART_H
#include "board.h"
#include "ring.h"
#define UART_TX_BUF_SIZE 1024
#define UART_RX_BUF_SIZE 2048
#define UART_DEFAULT_BAUD 115200
#if defined(CH570)
#define BRIDGE_UART UART1
#define BRIDGE_UART_IRQ UART1_IRQn
#define BRIDGE_UART_HANDLER UART1_IRQHandler
#define BRIDGE_TIMER TMR
#else
#define BRIDGE_UART UART0
#define BRIDGE_UART_IRQ UART0_IRQn
#define BRIDGE_UART_HANDLER UART0_IRQHandler
#define BRIDGE_TIMER TMR1
#endif
/* Debugger-visible counters, cumulative until reboot. */
typedef struct {
    volatile uint32_t rx_bytes, tx_bytes;
    volatile uint32_t rx_overrun, rx_frame_error, rx_parity_error;
    volatile uint32_t rx_ring_overflow, tx_ring_overflow;
    volatile uint32_t actual_baud;
} BridgeStats;
extern BridgeStats bridge_stats;
void bridge_init(void);
int bridge_set_line_coding(const uint8_t *coding);
void bridge_usb_out(const uint8_t *data, unsigned len);
int bridge_usb_in_complete(uint8_t *data);
void bridge_tick(void);
void bridge_capture_rx(void);
#endif

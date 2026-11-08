#ifndef USB_UART_BRIDGE_UART_H
#define USB_UART_BRIDGE_UART_H
#include "board.h"
#include "ring.h"
#define UART_TX_BUF_SIZE 1024
#define UART_RX_BUF_SIZE 2048
#define UART_DEFAULT_BAUD 115200
#define BRIDGE_UART UART1
#define BRIDGE_UART_IRQ UART1_IRQn
#define BRIDGE_UART_HANDLER UART1_IRQHandler
#define BRIDGE_TIMER TMR
/* Debugger-visible counters, cumulative until reboot. */
typedef struct {
    volatile uint32_t rx_bytes, tx_bytes;
    volatile uint32_t rx_overrun, rx_frame_error, rx_parity_error;
    volatile uint32_t rx_ring_overflow, tx_ring_overflow;
    volatile uint32_t actual_baud;
} BridgeStats;
extern BridgeStats bridge_stats;
extern volatile uint32_t bridge_ticks_per_ms;
void bridge_init(void);
int bridge_set_line_coding(const uint8_t *coding);
void bridge_usb_out(const uint8_t *data, unsigned len);
int bridge_usb_in_complete(uint8_t *data);
void bridge_tick(void);
void bridge_capture_rx(void);
#endif

#ifndef USB_UART_BRIDGE_BOARD_H
#define USB_UART_BRIDGE_BOARD_H

#include "ch32fun.h"

#ifndef BRIDGE_RS485
#define BRIDGE_RS485 0
#endif

#if !defined(CH570)
#error "usb_uart_bridge requires CH570"
#endif

#define UART_TX_PIN PA3
#define UART_RX_PIN PA2
#define RS485_DIR_PIN PA7
#if !BRIDGE_RS485
/* UART build: PA7 is the activity LED (active high, same polarity as the bootloader). */
#define BRIDGE_LED_PIN PA7
#endif

#endif

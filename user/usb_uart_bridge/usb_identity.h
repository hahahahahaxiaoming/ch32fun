#ifndef BRIDGE_USB_IDENTITY_H
#define BRIDGE_USB_IDENTITY_H
#include "board.h"
#if BRIDGE_RS485
#define BRIDGE_USB_PRODUCT u"USB RS485"
#define BRIDGE_SERIAL_MODE "RS485"
#else
#define BRIDGE_USB_PRODUCT u"USB UART"
#define BRIDGE_SERIAL_MODE "UART"
#endif
#define BRIDGE_SERIAL_PREFIX BRIDGE_SERIAL_MODE "-CH570-"
#define BRIDGE_SERIAL_CHARS (sizeof(BRIDGE_SERIAL_PREFIX) - 1 + 12)
struct bridge_serial_descriptor {
    uint8_t length;
    uint8_t type;
    uint16_t text[BRIDGE_SERIAL_CHARS];
};
extern struct bridge_serial_descriptor bridge_serial;
void usb_identity_init(void);
#endif

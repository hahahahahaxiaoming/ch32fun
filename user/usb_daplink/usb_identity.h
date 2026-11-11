#ifndef DAP_USB_IDENTITY_H
#define DAP_USB_IDENTITY_H
#include <stdint.h>
#define DAP_PRODUCT "CH570 CMSIS-DAP v2"
extern char dap_serial[17];
struct dap_serial_descriptor { uint8_t length, type; uint16_t text[16]; };
extern struct dap_serial_descriptor usb_serial;
void usb_identity_init(void);
#endif

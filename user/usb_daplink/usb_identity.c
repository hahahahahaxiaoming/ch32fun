#include "ch32fun.h"
#include "usb_identity.h"
char dap_serial[17] = "DAP-000000000000";
struct dap_serial_descriptor usb_serial;
void usb_identity_init(void) {
    volatile const uint32_t *rom = ROM_CFG_MAC_ADDR;
    uint32_t words[2] = {rom[0], rom[1]};
    const uint8_t *mac = (const uint8_t *)words;
    const char hex[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < 6; ++i) {
        dap_serial[4 + i*2] = hex[mac[i] >> 4];
        dap_serial[5 + i*2] = hex[mac[i] & 15];
    }
    usb_serial.length = sizeof(usb_serial);
    usb_serial.type = 3;
    for (unsigned i = 0; i < 16; ++i) usb_serial.text[i] = dap_serial[i];
}

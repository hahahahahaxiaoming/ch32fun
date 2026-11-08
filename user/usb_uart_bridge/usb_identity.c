#include "usb_identity.h"

struct bridge_serial_descriptor bridge_serial;

void usb_identity_init(void) {
    /* Read the same memory-mapped factory MAC as minichlink's CH570 HID backend. */
    volatile const uint32_t *rom_mac = ROM_CFG_MAC_ADDR;
    const uint32_t words[] = {rom_mac[0], rom_mac[1]};
    const uint8_t *mac = (const uint8_t *)words;
    const char *prefix = BRIDGE_SERIAL_PREFIX;
    const char hex[] = "0123456789ABCDEF";
    bridge_serial.length = sizeof(bridge_serial);
    bridge_serial.type = 3;
    unsigned pos = 0;
    for (; prefix[pos]; pos++) bridge_serial.text[pos] = prefix[pos];
    for (unsigned i = 0; i < 6; i++) {
        bridge_serial.text[pos++] = hex[mac[i] >> 4];
        bridge_serial.text[pos++] = hex[mac[i] & 15];
    }
}

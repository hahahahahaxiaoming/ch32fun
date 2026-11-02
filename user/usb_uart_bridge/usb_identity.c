#include "usb_identity.h"
/* The entire transaction must stay in RAM while the flash controller is in
 * manual mode. Otherwise LTO can inline this wrapper into flash-resident main
 * and return to unavailable flash immediately after rom_open_read(). */
void ch5xx_flash_cmd_get_rom_info(uint32_t addr, uint8_t *buf)
    __attribute__((section(".srodata.bridge_rom_info"), noinline, noclone));
#include "ch5xx_flash.h"
#ifndef ROM_CFG_MAC_ADDR
/* CH591 SDK ISP592.h ROM-info address (not declared by ch32fun yet). */
#define ROM_CFG_MAC_ADDR 0x7f018u
#endif

struct bridge_serial_descriptor bridge_serial;

void usb_identity_init(void) {
    /* Use the factory MAC, also used by WCH SDK GET_UNIQUE_ID, as unit identity.
     * Read via the repository's ROM-info helper before enabling USB interrupts. */
    uint32_t words[2] = {0};
    ch5xx_flash_cmd_get_rom_info((uint32_t)ROM_CFG_MAC_ADDR, (uint8_t *)words);
    const uint8_t *mac = (const uint8_t *)words;
    const char prefix[] = BRIDGE_SERIAL_PREFIX;
    const char hex[] = "0123456789ABCDEF";
    bridge_serial.length = sizeof(bridge_serial);
    bridge_serial.type = 3;
    unsigned pos = 0;
    for (; pos < sizeof(prefix) - 1; pos++) bridge_serial.text[pos] = prefix[pos];
    for (unsigned i = 0; i < 6; i++) {
        bridge_serial.text[pos++] = hex[mac[i] >> 4];
        bridge_serial.text[pos++] = hex[mac[i] & 15];
    }
}

#include "dap_validate.h"
/* Upstream parser has no length argument. Validate both input and worst-case
 * output BEFORE executing, including count fields supplied by the USB host.
 * Atomic/queued commands are not advertised or accepted by this transport. */
int dap_validate(const uint8_t *p, unsigned length) {
    if (!length || length > 64) return 0;
    unsigned need = 1, output = 0, pos, count;
    switch (p[0]) {
    case 0x00: case 0x02: case 0x13: need = 2; break;
    case 0x01: case 0x09: need = 3; break;
    case 0x03: case 0x0a: break;
    case 0x04: case 0x08: need = 6; break;
    case 0x10: need = 7; break;
    case 0x11: need = 5; break;
    case 0x12:
        if (length < 2) return 0;
        need = 2 + ((p[1] ? p[1] : 256) + 7) / 8;
        break;
    case 0x05:
        if (length < 3) return 0;
        pos = 3; output = 3;
        for (count = 0; count < p[2]; ++count) {
            if (pos >= length) return 0;
            uint8_t r = p[pos++];
            if (r & 0xc0) return 0; /* no timestamps/reserved bits */
            if (!(r & 2) || (r & 0x10)) pos += 4;
            if ((r & 2) && !(r & 0x10)) output += 4;
            if (pos > length || output > 64) return 0;
        }
        need = pos;
        break;
    case 0x06:
        if (length < 5 || (p[4] & 0xf0)) return 0;
        count = p[2] | ((unsigned)p[3] << 8);
        if (p[4] & 2) { if (count > 15) return 0; need = 5; }
        else { if (count > 14) return 0; need = 5 + 4 * count; }
        break;
    case 0x1d:
        if (length < 2) return 0;
        pos = 2; output = 2;
        for (count = 0; count < p[1]; ++count) {
            if (pos >= length) return 0;
            unsigned info = p[pos++];
            unsigned bits = info & 63;
            unsigned bytes = ((bits ? bits : 64) + 7) / 8;
            if (info & 0x40) return 0;
            if (info & 0x80) output += bytes; else pos += bytes;
            if (pos > length || output > 64) return 0;
        }
        need = pos;
        break;
    default: return 0;
    }
    return need <= length;
}

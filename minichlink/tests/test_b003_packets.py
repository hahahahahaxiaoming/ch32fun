"""Check real CH5xx streaming packet boundaries without a USB device."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PacketTest(unittest.TestCase):
    def test_windows_and_large_hid_buffers(self):
        source = (ROOT / "pgm-b003fun.c").read_text()
        start = source.index("static int B003FunBlockWrite(")
        end = source.index("static int B003FunWriteBinaryBlob(", start)
        harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minichlink.h"
#include "chips.h"
struct B003FunProgrammerStruct {
    void *internal;
    int scratchpad_data_size, commandplace, write_progress;
    uint8_t commandbuffer[8196];
};
struct MiniChlinkFunctions MCF;
static uint32_t total, packets, largest, expected;
static const uint8_t write_block_bin_v20x_v30x[1], write_block_bin[1], ch5xx_flash_write_block_bin[1];
int InternalUnlockFlash(void *dev, struct InternalState *state) { assert(0); return -1; }
static void ResetOp(struct B003FunProgrammerStruct *dev) { dev->commandplace = 4; }
static void WriteOpArb(struct B003FunProgrammerStruct *dev, const uint8_t *data, int size) {}
static void WriteOp4(struct B003FunProgrammerStruct *dev, uint32_t word) {
    memcpy(dev->commandbuffer + dev->commandplace, &word, 4);
    dev->commandplace += 4;
}
static void ch5xx_write_safe(void *dev, uint32_t addr, uint32_t value, int mode) {}
static void ch5xx_flash_close(void *dev) { assert(total == expected); }
static int CommitOp(struct B003FunProgrammerStruct *dev, int send, int receive) {
    if (!send) return 0; /* Initial resident-stub upload. */
    uint32_t end, size, key;
    memcpy(&end, dev->commandbuffer + 4, 4);
    memcpy(&size, dev->commandbuffer + 8, 4);
    memcpy(&key, dev->commandbuffer + 12 + size, 4);
    assert(end == 0x2000 + expected);
    assert(size && size % 256 == 0 && size <= dev->scratchpad_data_size);
    assert(send == size + 4 && key == 0x12345678);
    for (uint32_t i = 0; i < size; i++)
        assert(dev->commandbuffer[12+i] == (uint8_t)(total+i));
    total += size; packets++;
    if (size > largest) largest = size;
    return 0;
}
'''
        harness += source[start:end]
        harness += r'''
int main(void) {
    struct RiscVChip_s chip = {0}; chip.protocol = PROTOCOL_CH5xx;
    struct InternalState state = {0}; state.target_chip = &chip;
    struct B003FunProgrammerStruct dev = {0}; dev.internal = &state;
    expected = 180480; /* 1.0.2 app rounded to 256-byte flash pages. */
    uint8_t *image = malloc(expected);
    for (uint32_t i = 0; i < expected; i++) image[i] = (uint8_t)i;
    int budgets[] = {3968, 6144, 3072};
    for (int i = 0; i < 3; i++) {
        total = packets = largest = 0;
        dev.scratchpad_data_size = budgets[i];
        assert(B003FunBlockWrite(&dev, 0x2000, image, expected) == 0);
        assert(total == expected && packets > 1);
        assert(largest == budgets[i] / 256 * 256);
    }
    dev.scratchpad_data_size = 128;
    assert(B003FunBlockWrite(&dev, 0x2000, image, expected) == -10);
    dev.scratchpad_data_size = 3968;
    assert(B003FunBlockWrite(&dev, 0x2001, image, expected) == -10);
    assert(B003FunBlockWrite(&dev, 0x2000, image, expected-1) == -10);
    free(image);
    puts("CH5xx streaming page boundaries and payload integrity passed");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            c_file = path / "test.c"
            executable = path / ("test.exe" if os.name == "nt" else "test")
            c_file.write_text(harness)
            subprocess.run(["gcc", "-I", str(ROOT), str(c_file),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()

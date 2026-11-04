"""Exercise the real write dispatcher with mocked USB/flash operations."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WriteDispatchTest(unittest.TestCase):
    def test_chip_protocol_and_erase_failure(self):
        source = (ROOT / "pgm-b003fun.c").read_text()
        start = source.index("static int B003FunWriteBinaryBlob(")
        end = source.index("static int B003FunWriteHalfWord(", start)
        # Compile the production function, not a duplicate of its logic.
        dispatch = source[start:end]
        harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minichlink.h"
#include "chips.h"
#include "../ch32fun/ch32fun.h"
struct B003FunProgrammerStruct { void *internal; int scratchpad_size; };
struct MiniChlinkFunctions MCF;
static int unlocks, erases, writes, erase_result;
int InternalUnlockFlash(void *dev, struct InternalState *iss) {
    unlocks++; return -9;
}
static int erase(void *dev, uint32_t address, uint32_t size, int type) {
    assert(address == 0x2000 && size == 256 && type == 0);
    erases++; return erase_result;
}
static int B003FunBlockWrite(void *dev, uint32_t address, const uint8_t *data, uint32_t size) {
    assert(address == 0x2000 && size == 256 && data[0] == 0x6f);
    writes++; return 0;
}
static int B003FunBlockWrite64(void *dev, uint32_t address, const uint8_t *data) {
    assert(0); return -1;
}
static int InternalB003FunWriteBinaryBlob(void *dev, uint32_t address, uint32_t size, const uint8_t *data) {
    assert(0); return -1;
}
'''
        harness += dispatch
        harness += r'''
int main(int argc, char **argv) {
    uint8_t blob[256] = {0x6f};
    struct InternalState state = {0};
    struct B003FunProgrammerStruct dev = {&state, 4096};
    struct RiscVChip_s chip = {0};
    chip.sector_size = 256;
    state.target_chip = &chip;
    state.current_area = PROGRAM_AREA;
    MCF.Erase = erase;
    chip.protocol = PROTOCOL_CH5xx;
    assert(B003FunWriteBinaryBlob(&dev, 0x2000, sizeof(blob), blob) == 0);
    assert(unlocks == 0 && erases == 1 && writes == 1);
    erase_result = -7;
    assert(B003FunWriteBinaryBlob(&dev, 0x2000, sizeof(blob), blob) == -7);
    assert(unlocks == 0 && erases == 2 && writes == 1);
    chip.protocol = PROTOCOL_DEFAULT;
    assert(B003FunWriteBinaryBlob(&dev, 0x2000, sizeof(blob), blob) == -9);
    assert(unlocks == 1 && erases == 2 && writes == 1);
    puts("CH5xx bypass, CH32 unlock, erase failure checks passed");
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            c_file = path / "test.c"
            executable = path / ("test.exe" if os.name == "nt" else "test")
            c_file.write_text(harness)
            subprocess.run(["gcc", "-DCH32V003", "-I", str(ROOT),
                            str(c_file), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()

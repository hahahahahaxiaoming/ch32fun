"""Exercise production CH5xx read splitting and short unaligned reads."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReadTests(unittest.TestCase):
    def test_hid_limits_and_short_reads(self):
        source = (ROOT / "pgm-b003fun.c").read_text()
        start = source.index("static int B003FunReadBinaryBlob(")
        end = source.index("static int InternalB003FunBoot(", start)
        harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minichlink.h"
#include "chips.h"
#include "stubs/b003/ch5xx_flash_read_word.h"
#include "stubs/b003/ch5xx_flash_read_byte.h"
struct B003FunProgrammerStruct {
    void *internal;
    int scratchpad_size, scratchpad_data_size, commandplace;
    uint8_t commandbuffer[8196], respbuffer[8196];
};
static const uint8_t byte_wise_read_blob[48], half_wise_read_blob[48], word_wise_read_blob[48];
static void ch5xx_write_safe(void *dev, uint32_t addr, uint32_t value, int mode) {}
static void ch5xx_flash_close(void *dev) {}
static void ResetOp(struct B003FunProgrammerStruct *dev) { dev->commandplace = 4; }
static void WriteOpArb(struct B003FunProgrammerStruct *dev, const uint8_t *data, int size) {
    dev->commandplace += size;
}
static void WriteOp4(struct B003FunProgrammerStruct *dev, uint32_t word) {
    memcpy(dev->commandbuffer+dev->commandplace, &word, 4);
    dev->commandplace += 4;
}
static int CommitOp(struct B003FunProgrammerStruct *dev, int send, int receive) {
    uint32_t address, size;
    assert(receive + dev->commandplace + 4 <= dev->scratchpad_size);
    memcpy(&address, dev->commandbuffer+dev->commandplace-8, 4);
    memcpy(&size, dev->commandbuffer+dev->commandplace-4, 4);
    assert(size == receive);
    if(dev->commandplace == 4 + sizeof(ch5xx_flash_read_word_bin) + 8)
        assert(!(address % 4) && size && !(size % 4));
    for(uint32_t i=0;i<size;i++)
        dev->respbuffer[dev->commandplace-8+i] = (uint8_t)(address+i);
    return 0;
}
'''
        harness += source[start:end]
        harness += r'''
int main(void) {
    struct RiscVChip_s chip={0}; chip.protocol=PROTOCOL_CH5xx;
    struct InternalState state={0}; state.target_chip=&chip;
    struct B003FunProgrammerStruct dev={0}; dev.internal=&state;
    uint8_t output[180968+8];
    int sizes[]={128,1152,2176,3200,4096,5248,6272};
    for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++) {
        dev.scratchpad_size=sizes[s]; dev.scratchpad_data_size=sizes[s]-128;
        if(sizes[s] < 256) continue; /* Stub itself does not fit. */
        for(int area=0;area<2;area++) {
            state.current_area=area ? EEPROM_AREA : PROGRAM_AREA;
            for(int offset=0;offset<4;offset++) {
                unsigned lengths[]={1,2,3,4,5,180968};
                for(unsigned n=0;n<sizeof(lengths)/sizeof(lengths[0]);n++) {
                    unsigned length=lengths[n], address=(area ? 0x70000 : 0x2000)+offset;
                    memset(output,0xee,sizeof(output));
                    assert(B003FunReadBinaryBlob(&dev,address,length,output)==0);
                    for(unsigned i=0;i<length;i++) assert(output[i]==(uint8_t)(address+i));
                    assert(output[length]==0xee);
                }
            }
        }
    }
    puts("CH5xx readback: report bounds, data integrity and unaligned edges passed");
    return 0;
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

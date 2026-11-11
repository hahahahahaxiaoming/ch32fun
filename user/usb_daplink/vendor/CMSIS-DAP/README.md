# Vendored ARM CMSIS-DAP

Source: https://github.com/ARM-software/CMSIS-DAP

Pinned commit: `12636590eec66fae2d1bba4518749426ad5a4595`.
Files: `Firmware/Source/DAP.c`, `Firmware/Source/SW_DP.c`,
`Firmware/Include/DAP.h`, and upstream `LICENSE` (Apache-2.0).

Local changes:

- DAP.h: replace ARM Thumb delay assembly with RISC-V `addi`/`bnez`,
  and make zero delay a no-op instead of underflowing the loop counter.
- DAP.c: do not advertise atomic commands (transport does not implement queued
  execution); remove the unused local variable from DAP_SWJ_Clock.

- SW_DP.c: apply the optional `DAP_SWD_CODE` attribute to the SWD routines;
  the CH570 port runs these routines from RAM with speed optimization.

CH570 configuration and compiler vocabulary live outside this directory.
The portable parser is protected by `dap_validate.c` before processing USB data.

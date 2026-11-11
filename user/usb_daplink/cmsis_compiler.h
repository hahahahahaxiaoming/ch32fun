/* Minimal compiler vocabulary for the portable ARM CMSIS-DAP sources on RISC-V. */
#ifndef DAP_RISCV_COMPILER_H
#define DAP_RISCV_COMPILER_H
#define __STATIC_INLINE static inline
#define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
#define __WEAK __attribute__((weak))
#ifndef __ASM
#define __ASM __asm__
#endif
#define __NOP() __asm__ volatile("nop")
#endif

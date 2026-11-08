ENTRY(Init)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x1000, LENGTH = APP_FLASH_END - 0x1000
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = APP_RAM_SIZE
}

SECTIONS
{
    .init :
    {
        _sinit = .;
        KEEP(*(SORT_NONE(.init)))
        /* The front-of-flash bootloader checks this word at 0x1014. */
        . = 0x14;
        LONG(APP_VECTOR_TAG)
        _einit = .;
    } >FLASH AT>FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
    } >FLASH AT>FLASH

    .fini :
    {
        KEEP(*(SORT_NONE(.fini)))
        . = ALIGN(4);
    } >FLASH AT>FLASH
    PROVIDE(_etext = .);
    PROVIDE(_eitcm = .);

    .dalign :
    {
        . = ALIGN(4);
        PROVIDE(_data_vma = .);
    } >RAM AT>FLASH

    .dlalign :
    {
        . = ALIGN(4);
        PROVIDE(_data_lma = .);
    } >FLASH AT>FLASH

    .data :
    {
        . = ALIGN(4);
        __global_pointer$ = . + 0x3fc;
        /* mtvec requires a 1 KiB-aligned vector table on CH570. */
        KEEP(*(.data.vector_handler))
        *(.gnu.linkonce.r.*)
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
        *(.sdata .sdata.*)
        *(.sdata2*)
        *(.gnu.linkonce.s.*)
        . = ALIGN(8);
        *(.srodata.cst16)
        *(.srodata.cst8)
        *(.srodata.cst4)
        *(.srodata.cst2)
        *(.srodata .srodata.*)
        . = ALIGN(4);
        PROVIDE(_edata = .);
    } >RAM AT>FLASH
    ASSERT((InterruptVector & 0x3ff) == 0, "CH570 interrupt vector must be 1 KiB aligned")

    .highcode :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_vma_start = .);
        *(.highcode*)
        . = ALIGN(4);
        PROVIDE(_highcode_vma_end = .);
    } >RAM AT>FLASH
    PROVIDE(_highcode_lma = LOADADDR(.highcode));

    .bss :
    {
        . = ALIGN(4);
        PROVIDE(_sbss = .);
        *(.sbss*)
        *(.gnu.linkonce.sb.*)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON*)
        . = ALIGN(4);
        PROVIDE(_ebss = .);
    } >RAM

    PROVIDE(_end = _ebss);
    PROVIDE(end = .);
    PROVIDE(_eusrstack = ORIGIN(RAM) + LENGTH(RAM));

    /DISCARD/ :
    {
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
        *(.comment .comment.*)
    }
}

ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00101080, LENGTH = 8064
    RAM   (rwx) : ORIGIN = 0x00200000, LENGTH = 16K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.text.startup))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.ramfunc*)
        *(.data*)
        _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*) *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}

ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x00103000,
       "Void Bootloader exceeds its 8 KiB partition")
ASSERT(_ebss <= 0x00203E00, "Void Bootloader uses too much SRAM")

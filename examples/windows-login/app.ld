ENTRY(_start)
MEMORY
{
    /* Last two 128-byte pages are an A/B enrollment store. */
    FLASH (rx)  : ORIGIN = 0x00103080, LENGTH = 52864
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
        _sdata = .;
        *(.ramfunc*)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}
ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x0010FF00, "Windows Login overlaps enrollment pages")
ASSERT(_ebss <= 0x00203E00, "Windows Login uses too much SRAM")

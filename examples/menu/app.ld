ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00103080, LENGTH = 53120
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
        *(.data*)
        . = ALIGN(4);
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

ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x00110000,
       "Пример не помещается в раздел приложения")
ASSERT(_ebss <= 0x00203E00, "Пример использует слишком много SRAM")


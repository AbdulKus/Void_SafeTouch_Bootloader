#ifndef SAFETOUCH_HARDWARE_H
#define SAFETOUCH_HARDWARE_H

#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define PMC_BASE       0xFFFFFC00u
#define PMC_SCER       REG32(PMC_BASE + 0x00u)
#define PMC_PCER       REG32(PMC_BASE + 0x10u)
#define PMC_MOR        REG32(PMC_BASE + 0x20u)
#define PMC_PLLR       REG32(PMC_BASE + 0x2Cu)
#define PMC_MCKR       REG32(PMC_BASE + 0x30u)
#define PMC_SR         REG32(PMC_BASE + 0x68u)

#define PIOA_BASE      0xFFFFF400u
#define PIO_PER        REG32(PIOA_BASE + 0x00u)
#define PIO_PDR        REG32(PIOA_BASE + 0x04u)
#define PIO_OER        REG32(PIOA_BASE + 0x10u)
#define PIO_ODR        REG32(PIOA_BASE + 0x14u)
#define PIO_SODR       REG32(PIOA_BASE + 0x30u)
#define PIO_CODR       REG32(PIOA_BASE + 0x34u)
#define PIO_PDSR       REG32(PIOA_BASE + 0x3Cu)
#define PIO_PUER       REG32(PIOA_BASE + 0x64u)
#define PIO_ASR        REG32(PIOA_BASE + 0x70u)

#define BUTTON_GREEN   (1u << 21)
#define BUTTON_RED     (1u << 19)
#define LED_GREEN      (1u << 25)
#define LED_RED        (1u << 26)
#define BACKLIGHT      (1u << 18)
#define USB_PULLUP     (1u << 16)

#define LCD_CS         (1u << 11)
#define LCD_RST        (1u << 3)
#define LCD_A0         (1u << 4)
#define LCD_CLK        (1u << 14)
#define LCD_MOSI       (1u << 13)
#define LCD_MASK       (LCD_CS | LCD_RST | LCD_A0 | LCD_CLK | LCD_MOSI)

#define CARD_DETECT    (1u << 5)
#define CARD_RESET     (1u << 7)
#define CARD_POWER     (1u << 8)
#define CARD_IO        (1u << 22)
#define CARD_CLOCK     (1u << 23)

static inline void delay(volatile uint32_t count)
{
    while (count--) __asm__ volatile ("nop");
}

void clocks_init(void);

#endif

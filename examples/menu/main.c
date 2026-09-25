#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define PIOA_BASE 0xFFFFF400u
#define PIO_PER  REG32(PIOA_BASE + 0x00u)
#define PIO_OER  REG32(PIOA_BASE + 0x10u)
#define PIO_ODR  REG32(PIOA_BASE + 0x14u)
#define PIO_SODR REG32(PIOA_BASE + 0x30u)
#define PIO_CODR REG32(PIOA_BASE + 0x34u)
#define PIO_PDSR REG32(PIOA_BASE + 0x3Cu)
#define PIO_PUER REG32(PIOA_BASE + 0x64u)

#define LCD_CS   (1u << 11)
#define LCD_RST  (1u << 3)
#define LCD_A0   (1u << 4)
#define LCD_CLK  (1u << 14)
#define LCD_MOSI (1u << 13)
#define LCD_MASK (LCD_CS | LCD_RST | LCD_A0 | LCD_CLK | LCD_MOSI)

#define BUTTON_YES  (1u << 21)
#define BUTTON_NO   (1u << 19)
#define BUTTON_MASK (BUTTON_YES | BUTTON_NO)
#define LED_GREEN   (1u << 25)
#define LED_RED     (1u << 26)
#define LED_MASK    (LED_GREEN | LED_RED)

static uint8_t framebuffer[8][132];

struct glyph { char character; uint8_t columns[5]; };
static const struct glyph font[] = {
    {'A',{0x7E,0x11,0x11,0x11,0x7E}}, {'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'D',{0x7F,0x41,0x41,0x22,0x1C}}, {'E',{0x7F,0x49,0x49,0x49,0x41}},
    {'F',{0x7F,0x09,0x09,0x09,0x01}}, {'G',{0x3E,0x41,0x49,0x49,0x7A}},
    {'H',{0x7F,0x08,0x08,0x08,0x7F}}, {'M',{0x7F,0x02,0x04,0x02,0x7F}},
    {'N',{0x7F,0x02,0x04,0x08,0x7F}}, {'O',{0x3E,0x41,0x41,0x41,0x3E}},
    {'R',{0x7F,0x09,0x19,0x29,0x46}}, {'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}}, {'>',{0x08,0x14,0x22,0x41,0x00}},
    {'*',{0x14,0x08,0x3E,0x08,0x14}}
};

static void set_bits(uint32_t mask) { PIO_SODR = mask; }
static void clear_bits(uint32_t mask) { PIO_CODR = mask; }
static void delay(volatile uint32_t count)
{
    while (count--) __asm__ volatile ("nop");
}

static void spi_write(uint8_t value)
{
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (value & 0x80u) set_bits(LCD_MOSI);
        else clear_bits(LCD_MOSI);
        delay(2);
        set_bits(LCD_CLK);
        delay(2);
        clear_bits(LCD_CLK);
        value <<= 1;
    }
}

static void lcd_command(uint8_t value)
{
    clear_bits(LCD_CS | LCD_A0);
    spi_write(value);
    set_bits(LCD_CS);
}

static void lcd_data(uint8_t value)
{
    clear_bits(LCD_CS);
    set_bits(LCD_A0);
    spi_write(value);
    set_bits(LCD_CS);
}

static const uint8_t *find_glyph(char character)
{
    for (unsigned i = 0; i < sizeof(font) / sizeof(font[0]); ++i)
        if (font[i].character == character) return font[i].columns;
    return 0;
}

static void clear_framebuffer(void)
{
    for (unsigned page = 0; page < 8; ++page)
        for (unsigned column = 0; column < 132; ++column)
            framebuffer[page][column] = 0;
}

static void set_pixel(unsigned x, unsigned y)
{
    if (x < 132 && y < 64)
        framebuffer[y >> 3][x] |= (uint8_t)(1u << (y & 7u));
}

static void draw_text(unsigned x, unsigned y, const char *text, unsigned scale)
{
    while (*text) {
        const uint8_t *glyph = find_glyph(*text++);
        if (glyph) {
            for (unsigned gx = 0; gx < 5; ++gx)
                for (unsigned gy = 0; gy < 7; ++gy)
                    if (glyph[gx] & (1u << gy))
                        for (unsigned sx = 0; sx < scale; ++sx)
                            for (unsigned sy = 0; sy < scale; ++sy)
                                set_pixel(x + gx * scale + sx, y + gy * scale + sy);
        }
        x += 6 * scale;
    }
}

static void update_display(void)
{
    for (unsigned page = 0; page < 8; ++page) {
        lcd_command((uint8_t)(0xB0u | page));
        lcd_command(0x10);
        lcd_command(0x00);
        for (unsigned column = 0; column < 132; ++column)
            lcd_data(framebuffer[page][column]);
    }
}

static void render_menu(unsigned cursor, unsigned active)
{
    static const char *const items[] = {"OFF", "GREEN", "RED", "BOTH"};
    clear_framebuffer();
    draw_text(38, 0, "MENU", 2);
    for (unsigned item = 0; item < 4; ++item) {
        unsigned y = 20 + item * 10;
        if (item == cursor) draw_text(4, y, ">", 1);
        draw_text(16, y, items[item], 1);
        if (item == active) draw_text(118, y, "*", 1);
    }
    update_display();
}

static void update_leds(unsigned mode, unsigned phase)
{
    uint32_t on = 0;
    if (phase) {
        if (mode == 1 || mode == 3) on |= LED_GREEN;
        if (mode == 2 || mode == 3) on |= LED_RED;
    }
    set_bits(LED_MASK);
    clear_bits(on); /* Светодиоды включаются низким уровнем. */
}

int main(void)
{
    REG32(0xFFFFFD44u) = 0x00008000u; /* Отключить watchdog. */
    REG32(0xFFFFFC10u) = (1u << 2);   /* Тактирование PIOA. */
    PIO_PER = LCD_MASK | BUTTON_MASK | LED_MASK;
    PIO_OER = LCD_MASK | LED_MASK;
    PIO_ODR = BUTTON_MASK;
    PIO_PUER = BUTTON_MASK;
    set_bits(LCD_CS | LCD_RST | LED_MASK);
    clear_bits(LCD_A0 | LCD_CLK | LCD_MOSI);

    clear_bits(LCD_RST);
    delay(2000);
    set_bits(LCD_RST);
    delay(2000);
    lcd_command(0xAE); lcd_command(0xA2); lcd_command(0xA1);
    lcd_command(0xC0); lcd_command(0x40); lcd_command(0x2F);
    delay(10000);
    lcd_command(0x27); lcd_command(0x81); lcd_command(0x04);
    lcd_command(0xA6); lcd_command(0xA4); lcd_command(0xAF);

    unsigned cursor = 0, active = 0, phase = 0, blink_counter = 0;
    render_menu(cursor, active);
    for (;;) {
        if (!(PIO_PDSR & BUTTON_NO)) {
            cursor = (cursor + 1u) & 3u;
            render_menu(cursor, active);
            delay(50000);
            while (!(PIO_PDSR & BUTTON_NO)) { }
        }
        if (!(PIO_PDSR & BUTTON_YES)) {
            active = cursor;
            render_menu(cursor, active);
            delay(50000);
            while (!(PIO_PDSR & BUTTON_YES)) { }
        }
        if (++blink_counter >= 250000u) {
            blink_counter = 0;
            phase ^= 1u;
            update_leds(active, phase);
        }
    }
}


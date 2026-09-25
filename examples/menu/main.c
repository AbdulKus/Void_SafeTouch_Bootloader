#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define PMC_BASE  0xFFFFFC00u
#define PMC_PCER  REG32(PMC_BASE + 0x10u)

#define PIOA_BASE 0xFFFFF400u
#define PIO_PER   REG32(PIOA_BASE + 0x00u)
#define PIO_PDR   REG32(PIOA_BASE + 0x04u)
#define PIO_OER   REG32(PIOA_BASE + 0x10u)
#define PIO_ODR   REG32(PIOA_BASE + 0x14u)
#define PIO_SODR  REG32(PIOA_BASE + 0x30u)
#define PIO_CODR  REG32(PIOA_BASE + 0x34u)
#define PIO_PDSR  REG32(PIOA_BASE + 0x3Cu)
#define PIO_PUER  REG32(PIOA_BASE + 0x64u)
#define PIO_ASR   REG32(PIOA_BASE + 0x70u)

#define USART1_BASE 0xFFFC4000u
#define US_CR       REG32(USART1_BASE + 0x00u)
#define US_MR       REG32(USART1_BASE + 0x04u)
#define US_IDR      REG32(USART1_BASE + 0x0Cu)
#define US_CSR      REG32(USART1_BASE + 0x14u)
#define US_RHR      REG32(USART1_BASE + 0x18u)
#define US_BRGR     REG32(USART1_BASE + 0x20u)
#define US_RTOR     REG32(USART1_BASE + 0x24u)
#define US_TTGR     REG32(USART1_BASE + 0x28u)
#define US_FIDI     REG32(USART1_BASE + 0x40u)

#define LCD_CS       (1u << 11)
#define LCD_RST      (1u << 3)
#define LCD_A0       (1u << 4)
#define LCD_CLK      (1u << 14)
#define LCD_MOSI     (1u << 13)
#define LCD_MASK     (LCD_CS | LCD_RST | LCD_A0 | LCD_CLK | LCD_MOSI)

#define BUTTON_YES   (1u << 21)
#define BUTTON_NO    (1u << 19)
#define BUTTON_MASK  (BUTTON_YES | BUTTON_NO)
#define LED_GREEN    (1u << 25)
#define LED_RED      (1u << 26)
#define LED_MASK     (LED_GREEN | LED_RED)

/* Confirmed board wiring, expressed as PIO lines rather than package pins. */
#define BACKLIGHT    (1u << 18) /* MCU pin 10, transistor switch, active-high. */
#define CARD_DETECT  (1u << 5)  /* MCU pin 35, closes to ground. */
#define CARD_RESET   (1u << 7)  /* MCU pin 32, ISO 7816 C2. */
#define CARD_POWER   (1u << 8)  /* MCU pin 31, ISO 7816 C1. */
#define CARD_IO      (1u << 22) /* MCU pin 14, USART1 TXD1 / ISO 7816 I/O. */
#define CARD_CLOCK   (1u << 23) /* MCU pin 15, USART1 SCK1 / ISO 7816 clock. */
#define CARD_GPIO    (CARD_DETECT | CARD_RESET | CARD_POWER)

#define MENU_ITEMS   7u
#define VISIBLE_ROWS 4u
#define ATR_MAX      32u

enum screen_mode { SCREEN_MENU, SCREEN_CARD };
enum card_status { CARD_ABSENT, CARD_READING, CARD_READY, CARD_NO_RESPONSE };

static uint8_t framebuffer[8][132];
static uint8_t atr[ATR_MAX];
static unsigned atr_length;
static enum card_status current_card_status;

struct glyph { char character; uint8_t columns[5]; };
static const struct glyph font[] = {
    {' ',{0x00,0x00,0x00,0x00,0x00}}, {'-',{0x08,0x08,0x08,0x08,0x08}},
    {'0',{0x3E,0x51,0x49,0x45,0x3E}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}}, {'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}}, {'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}}, {'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}}, {'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'I',{0x00,0x41,0x7F,0x41,0x00}}, {'J',{0x20,0x40,0x41,0x3F,0x01}},
    {'K',{0x7F,0x08,0x14,0x22,0x41}}, {'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x04,0x02,0x7F}}, {'N',{0x7F,0x02,0x04,0x08,0x7F}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}}, {'P',{0x7F,0x09,0x09,0x09,0x06}},
    {'Q',{0x3E,0x41,0x51,0x21,0x5E}}, {'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}}, {'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}}, {'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {'W',{0x3F,0x40,0x38,0x40,0x3F}}, {'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x07,0x08,0x70,0x08,0x07}}, {'Z',{0x61,0x51,0x49,0x45,0x43}},
    {'>',{0x08,0x14,0x22,0x41,0x00}}, {'*',{0x14,0x08,0x3E,0x08,0x14}}
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
    return font[0].columns;
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
        for (unsigned gx = 0; gx < 5; ++gx)
            for (unsigned gy = 0; gy < 7; ++gy)
                if (glyph[gx] & (1u << gy))
                    for (unsigned sx = 0; sx < scale; ++sx)
                        for (unsigned sy = 0; sy < scale; ++sy)
                            set_pixel(x + gx * scale + sx, y + gy * scale + sy);
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

static char hex_digit(unsigned value)
{
    value &= 15u;
    return (char)(value < 10u ? '0' + value : 'A' + value - 10u);
}

static void hex8(char *output, uint32_t value)
{
    for (unsigned i = 0; i < 8; ++i) {
        output[7u - i] = hex_digit(value);
        value >>= 4;
    }
    output[8] = 0;
}

static uint32_t crc32(const uint8_t *data, unsigned length)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (length--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void render_menu(unsigned cursor, unsigned led_mode, unsigned contrast,
                        unsigned backlight_on)
{
    static const char *const names[] = {
        "LED OFF", "LED GREEN", "LED RED", "LED BOTH",
        "CONTRAST", "BACKLIGHT", "SMART CARD"
    };
    unsigned first = 0;
    char value[3];

    if (cursor >= VISIBLE_ROWS)
        first = cursor - VISIBLE_ROWS + 1u;

    clear_framebuffer();
    draw_text(38, 0, "MENU", 2);
    for (unsigned row = 0; row < VISIBLE_ROWS && first + row < MENU_ITEMS; ++row) {
        unsigned item = first + row;
        unsigned y = 20u + row * 10u;
        if (item == cursor) draw_text(1, y, ">", 1);
        draw_text(10, y, names[item], 1);
        if (item < 4u && item == led_mode) draw_text(124, y, "*", 1);
        if (item == 4u) {
            value[0] = hex_digit(contrast >> 4);
            value[1] = hex_digit(contrast);
            value[2] = 0;
            draw_text(118, y, value, 1);
        }
        if (item == 5u) draw_text(112, y, backlight_on ? "ON" : "OFF", 1);
    }
    update_display();
}

static void render_card(void)
{
    char id[9];
    char raw[17];

    clear_framebuffer();
    draw_text(0, 0, "SMART CARD", 2);
    if (current_card_status == CARD_ABSENT) {
        draw_text(24, 28, "CARD ABSENT", 1);
        draw_text(9, 48, "NO BACK YES RETRY", 1);
    } else if (current_card_status == CARD_READING) {
        draw_text(39, 28, "READING", 1);
    } else if (current_card_status == CARD_NO_RESPONSE) {
        draw_text(30, 28, "NO RESPONSE", 1);
        draw_text(9, 48, "NO BACK YES RETRY", 1);
    } else {
        hex8(id, crc32(atr, atr_length));
        draw_text(0, 20, "ID", 1);
        draw_text(18, 20, id, 1);
        unsigned shown = atr_length < 8u ? atr_length : 8u;
        for (unsigned i = 0; i < shown; ++i) {
            raw[i * 2u] = hex_digit(atr[i] >> 4);
            raw[i * 2u + 1u] = hex_digit(atr[i]);
        }
        raw[shown * 2u] = 0;
        draw_text(0, 34, "ATR", 1);
        draw_text(24, 34, raw, 1);
        draw_text(9, 50, "NO BACK YES RETRY", 1);
    }
    update_display();
}

static void update_leds(unsigned mode, unsigned phase)
{
    uint32_t on = 0;
    if (phase) {
        if (mode == 1u || mode == 3u) on |= LED_GREEN;
        if (mode == 2u || mode == 3u) on |= LED_RED;
    }
    set_bits(LED_MASK);
    clear_bits(on); /* Board LEDs are active-low. */
}

static int card_present(void)
{
    return !(PIO_PDSR & CARD_DETECT);
}

static void card_off(void)
{
    US_CR = (1u << 5) | (1u << 7); /* RXDIS | TXDIS. */
    PIO_PER = CARD_IO | CARD_CLOCK;
    PIO_OER = CARD_IO | CARD_CLOCK | CARD_RESET | CARD_POWER;
    clear_bits(CARD_IO | CARD_CLOCK | CARD_RESET | CARD_POWER);
}

static void card_interface_init(void)
{
    PMC_PCER = (1u << 7); /* USART1 peripheral clock. */
    PIO_ASR = CARD_IO | CARD_CLOCK;
    PIO_PDR = CARD_IO | CARD_CLOCK;
    US_CR = (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 8);
    US_IDR = 0xFFFFFFFFu;
    US_MR = 0x04u | (1u << 18) | (3u << 24); /* ISO7816 T=0, clock out. */
    US_BRGR = 12u;  /* 48 MHz / 12 = 4 MHz card clock. */
    US_FIDI = 372u; /* Default Fi/Di after reset. */
    US_RTOR = 0u;
    US_TTGR = 5u;
    US_CR = (1u << 4) | (1u << 6); /* RXEN | TXEN. */
}

static int card_receive(uint8_t *value, uint32_t timeout)
{
    while (timeout--) {
        uint32_t status = US_CSR;
        if (status & ((1u << 5) | (1u << 6) | (1u << 7))) {
            US_CR = (1u << 8); /* RSTSTA. */
            return 0;
        }
        if (status & 1u) {
            *value = (uint8_t)US_RHR;
            return 1;
        }
    }
    return 0;
}

static int card_read_atr(void)
{
    unsigned index = 0;
    unsigned historical;
    unsigned group = 0;
    uint8_t y;
    int tck_required = 0;

    atr_length = 0;
    if (!card_present()) return 0;

    card_off();
    delay(20000);
    card_interface_init();
    set_bits(CARD_POWER);
    delay(50000);
    set_bits(CARD_RESET);

    if (!card_receive(&atr[index++], 2000000u)) goto fail;
    if (atr[0] != 0x3Bu && atr[0] != 0x3Fu) goto fail;
    if (!card_receive(&atr[index++], 1000000u)) goto fail;
    historical = atr[1] & 15u;
    y = atr[1] >> 4;

    do {
        uint8_t td = 0;
        if ((y & 1u) && (index >= ATR_MAX || !card_receive(&atr[index++], 1000000u))) goto fail;
        if ((y & 2u) && (index >= ATR_MAX || !card_receive(&atr[index++], 1000000u))) goto fail;
        if ((y & 4u) && (index >= ATR_MAX || !card_receive(&atr[index++], 1000000u))) goto fail;
        if (y & 8u) {
            if (index >= ATR_MAX || !card_receive(&td, 1000000u)) goto fail;
            atr[index++] = td;
            if ((td & 15u) != 0u) tck_required = 1;
            y = td >> 4;
        } else {
            y = 0;
        }
        if (++group > 7u) goto fail;
    } while (y);

    while (historical--) {
        if (index >= ATR_MAX || !card_receive(&atr[index++], 1000000u)) goto fail;
    }
    if (tck_required) {
        if (index >= ATR_MAX || !card_receive(&atr[index++], 1000000u)) goto fail;
    }
    atr_length = index;
    return 1;

fail:
    atr_length = 0;
    card_off();
    return 0;
}

static void refresh_card(void)
{
    if (!card_present()) {
        card_off();
        current_card_status = CARD_ABSENT;
        render_card();
        return;
    }
    current_card_status = CARD_READING;
    render_card();
    current_card_status = card_read_atr() ? CARD_READY : CARD_NO_RESPONSE;
    render_card();
}

int main(void)
{
    REG32(0xFFFFFD44u) = 0x00008000u; /* Disable watchdog. */
    PMC_PCER = (1u << 2);             /* PIOA peripheral clock. */
    PIO_PER = LCD_MASK | BUTTON_MASK | LED_MASK | BACKLIGHT | CARD_GPIO;
    PIO_OER = LCD_MASK | LED_MASK | BACKLIGHT | CARD_RESET | CARD_POWER;
    PIO_ODR = BUTTON_MASK | CARD_DETECT;
    PIO_PUER = BUTTON_MASK | CARD_DETECT;
    set_bits(LCD_CS | LCD_RST | LED_MASK | BACKLIGHT);
    clear_bits(LCD_A0 | LCD_CLK | LCD_MOSI | CARD_RESET | CARD_POWER);

    clear_bits(LCD_RST);
    delay(2000);
    set_bits(LCD_RST);
    delay(2000);
    lcd_command(0xAE); lcd_command(0xA2); lcd_command(0xA1);
    lcd_command(0xC0); lcd_command(0x40); lcd_command(0x2F);
    delay(10000);
    lcd_command(0x27); lcd_command(0x81); lcd_command(0x04);
    lcd_command(0xA6); lcd_command(0xA4); lcd_command(0xAF);

    unsigned cursor = 0, led_mode = 0, phase = 0, blink_counter = 0;
    unsigned contrast = 4, backlight_on = 1;
    enum screen_mode screen = SCREEN_MENU;
    render_menu(cursor, led_mode, contrast, backlight_on);

    for (;;) {
        if (!(PIO_PDSR & BUTTON_NO)) {
            if (screen == SCREEN_CARD) {
                card_off();
                screen = SCREEN_MENU;
            } else {
                if (++cursor == MENU_ITEMS) cursor = 0;
            }
            render_menu(cursor, led_mode, contrast, backlight_on);
            delay(50000);
            while (!(PIO_PDSR & BUTTON_NO)) { }
        }
        if (!(PIO_PDSR & BUTTON_YES)) {
            if (screen == SCREEN_CARD) {
                refresh_card();
            } else if (cursor < 4u) {
                led_mode = cursor;
                render_menu(cursor, led_mode, contrast, backlight_on);
            } else if (cursor == 4u) {
                contrast = (contrast + 1u) & 63u;
                lcd_command(0x81); lcd_command((uint8_t)contrast);
                render_menu(cursor, led_mode, contrast, backlight_on);
            } else if (cursor == 5u) {
                backlight_on ^= 1u;
                if (backlight_on) set_bits(BACKLIGHT); else clear_bits(BACKLIGHT);
                render_menu(cursor, led_mode, contrast, backlight_on);
            } else {
                screen = SCREEN_CARD;
                refresh_card();
            }
            delay(50000);
            while (!(PIO_PDSR & BUTTON_YES)) { }
        }
        if (++blink_counter >= 250000u) {
            blink_counter = 0;
            phase ^= 1u;
            update_leds(led_mode, phase);
        }
    }
}

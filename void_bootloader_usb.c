#include <stdint.h>
#include "void_image.h"

#ifndef VOID_STAGE
#define VOID_STAGE 0
#endif

#define REG32(address) (*(volatile uint32_t *)(address))

/* Clock and reset controller. */
#define PMC_BASE       0xFFFFFC00u
#define PMC_SCER       REG32(PMC_BASE + 0x00u)
#define PMC_PCER       REG32(PMC_BASE + 0x10u)
#define PMC_MCKR       REG32(PMC_BASE + 0x30u)
#define PMC_PLLR       REG32(PMC_BASE + 0x2Cu)
#define PMC_MOR        REG32(PMC_BASE + 0x20u)
#define PMC_SR         REG32(PMC_BASE + 0x68u)
#define WDT_MR         REG32(0xFFFFFD44u)
#define MC_FMR         REG32(0xFFFFFF60u)

/* PIOA. PA16 drives the board's 1.5K USB D+ pull-up through a transistor. */
#define PIOA_BASE      0xFFFFF400u
#define PIO_PER        REG32(PIOA_BASE + 0x00u)
#define PIO_OER        REG32(PIOA_BASE + 0x10u)
#define PIO_ODR        REG32(PIOA_BASE + 0x14u)
#define PIO_SODR       REG32(PIOA_BASE + 0x30u)
#define PIO_CODR       REG32(PIOA_BASE + 0x34u)
#define PIO_PDSR       REG32(PIOA_BASE + 0x3Cu)
#define PIO_PUER       REG32(PIOA_BASE + 0x64u)
#define USB_PULLUP     (1u << 16)
#define BUTTON_GREEN   (1u << 21)
#define BUTTON_RED     (1u << 19)
#define LCD_CS         (1u << 11)
#define LCD_RST        (1u << 3)
#define LCD_A0         (1u << 4)
#define LCD_CLK        (1u << 14)
#define LCD_MOSI       (1u << 13)
#define LCD_MASK       (LCD_CS | LCD_RST | LCD_A0 | LCD_CLK | LCD_MOSI)

#define MC_FCR         REG32(0xFFFFFF64u)
#define MC_FSR         REG32(0xFFFFFF68u)
#define MC_FRDY        (1u << 0)
#define MC_LOCKE       (1u << 2)
#define MC_PROGE       (1u << 3)
#define MC_KEY         (0x5Au << 24)
#define MC_FCMD_WP     0x01u

/* USB Device Port (UDP). */
#define UDP_BASE       0xFFFB0000u
#define UDP_GLB_STAT   REG32(UDP_BASE + 0x04u)
#define UDP_FADDR      REG32(UDP_BASE + 0x08u)
#define UDP_IDR        REG32(UDP_BASE + 0x14u)
#define UDP_ISR        REG32(UDP_BASE + 0x1Cu)
#define UDP_ICR        REG32(UDP_BASE + 0x20u)
#define UDP_RST_EP     REG32(UDP_BASE + 0x28u)
#define UDP_CSR(ep)    REG32(UDP_BASE + 0x30u + ((ep) * 4u))
#define UDP_FDR(ep)    REG32(UDP_BASE + 0x50u + ((ep) * 4u))
#define UDP_TXVC       REG32(UDP_BASE + 0x74u)

#define UDP_TXCOMP       (1u << 0)
#define UDP_RX_DATA_BK0  (1u << 1)
#define UDP_RXSETUP      (1u << 2)
#define UDP_STALLSENT    (1u << 3)
#define UDP_TXPKTRDY     (1u << 4)
#define UDP_FORCESTALL   (1u << 5)
#define UDP_RX_DATA_BK1  (1u << 6)
#define UDP_DIR          (1u << 7)
#define UDP_EPTYPE_CTRL  (0u << 8)
#define UDP_EPTYPE_INT_OUT (3u << 8)
#define UDP_EPTYPE_INT_IN  (7u << 8)
#define UDP_EPEDS        (1u << 15)
#define UDP_RXBYTECNT(v) (((v) >> 16) & 0x7FFu)
#define UDP_FEN          (1u << 8)
#define UDP_FADDEN       (1u << 0)
#define UDP_CONFG        (1u << 1)
#define UDP_ENDBUSRES    (1u << 12)

#define EP0_SIZE 8u
#define REPORT_SIZE 64u
#define DBG(index) REG32(0x00203F80u + ((index) * 4u))
#define WARM_RECOVERY REG32(0x00203F70u)
#define WARM_RECOVERY_MAGIC 0x56575230u
#define WARM_BOOT_MAGIC     0x56575231u

#ifdef VOID_DEBUG
#define DEBUG_INC(index) (DBG(index)++)
#define DEBUG_SET(index, value) (DBG(index) = (value))
#else
#define DEBUG_INC(index) ((void)0)
#define DEBUG_SET(index, value) ((void)0)
#endif

/* Experimental VID/PID. A public release must receive its own pid.codes PID. */
#define VOID_VID 0x1209u
#define VOID_PID 0xB007u

struct setup_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

static const uint8_t device_descriptor[] = {
    18, 1, 0x00, 0x02,
    0x00, 0x00, 0x00, EP0_SIZE,
    (uint8_t)VOID_VID, (uint8_t)(VOID_VID >> 8),
    (uint8_t)VOID_PID, (uint8_t)(VOID_PID >> 8),
    0x00, 0x01,
    1, 2, 3, 1
};

static const uint8_t hid_report_descriptor[] = {
    0x06, 0x00, 0xFF,       /* Usage Page (vendor defined) */
    0x09, 0x01,             /* Usage 1 */
    0xA1, 0x01,             /* Application collection */
    0x15, 0x00,             /* Logical minimum 0 */
    0x26, 0xFF, 0x00,       /* Logical maximum 255 */
    0x75, 0x08,             /* Report size 8 */
    0x95, REPORT_SIZE,      /* Report count 64 */
    0x09, 0x01,
    0x81, 0x02,             /* Input report */
    0x95, REPORT_SIZE,
    0x09, 0x01,
    0x91, 0x02,             /* Output report */
    0xC0
};

static const uint8_t hid_descriptor[] = {
    9, 0x21, 0x11, 0x01, 0, 1, 0x22,
    sizeof(hid_report_descriptor), 0
};

static const uint8_t configuration_descriptor[] = {
    /* Configuration: 9 + interface 9 + HID 9 + two endpoints 14 = 41. */
    9, 2, 41, 0, 1, 1, 0, 0x80, 25,
    /* Vendor-defined HID interface. */
    9, 4, 0, 0, 2, 3, 0, 0, 4,
    /* HID descriptor. */
    9, 0x21, 0x11, 0x01, 0, 1, 0x22,
    sizeof(hid_report_descriptor), 0,
    /* EP1 interrupt IN, 64 bytes, 10ms. */
    7, 5, 0x81, 3, REPORT_SIZE, 0, 10,
    /* EP2 interrupt OUT, 64 bytes, 10ms. */
    7, 5, 0x02, 3, REPORT_SIZE, 0, 10
};

static const uint8_t string_language[] = { 4, 3, 0x09, 0x04 };
static const uint8_t string_manufacturer[] = {
    10, 3, 'V',0,'o',0,'i',0,'d',0
};
static const uint8_t string_product[] = {
    32, 3,
    'V',0,'o',0,'i',0,'d',0,' ',0,'B',0,'o',0,'o',0,'t',0,
    'l',0,'o',0,'a',0,'d',0,'e',0,'r',0
};
static const uint8_t string_serial[] = {
    16, 3, 'V',0,'B',0,'-',0,'0',0,'0',0,'0',0,'1',0
};
static const uint8_t string_interface[] = {
    34, 3,
    'V',0,'o',0,'i',0,'d',0,' ',0,'U',0,'p',0,'d',0,'a',0,
    't',0,'e',0,'r',0,' ',0,'H',0,'I',0,'D',0
};

static uint8_t ep0_reply_buffer[REPORT_SIZE];
static const uint8_t *ep0_data;
static uint16_t ep0_length;
static uint16_t ep0_offset;
static uint8_t ep0_zlp;
static uint8_t ep0_active;
static uint8_t pending_address;
static uint8_t address_pending;
static uint8_t pending_configuration;
static uint8_t configuration_pending;
static uint8_t configured;
static uint8_t out_report[REPORT_SIZE];
static uint8_t in_report[REPORT_SIZE];
static uint8_t in_report_pending;

enum {
    CMD_INFO = 0x01,
    CMD_BEGIN = 0x10,
    CMD_DATA = 0x11,
    CMD_END = 0x12,
    CMD_ABORT = 0x13,
    CMD_BOOT = 0x20,
    CMD_RECOVERY = 0x21
};

enum {
    ST_OK = 0,
    ST_BAD_COMMAND = 1,
    ST_BAD_TARGET = 2,
    ST_BAD_LENGTH = 3,
    ST_BAD_STATE = 4,
    ST_BAD_OFFSET = 5,
    ST_FLASH = 6,
    ST_CRC = 7,
    ST_BAD_IMAGE = 8
};

struct update_state {
    uint32_t base;
    uint32_t partition_size;
    uint32_t magic;
    uint32_t length;
    uint32_t expected_crc;
    uint32_t running_crc;
    uint32_t version;
    uint32_t received;
    uint32_t page_address;
    uint8_t page[VOID_PAGE_SIZE];
    uint8_t page_used;
    uint8_t active;
};

static struct update_state update;
static uint8_t deferred_action;

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t crc32_byte(uint32_t crc, uint8_t value)
{
    crc ^= value;
    for (unsigned i = 0; i < 8; ++i)
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    return crc;
}

static uint32_t crc32_memory(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (length--) crc = crc32_byte(crc, *data++);
    return crc ^ 0xFFFFFFFFu;
}

/* This complete routine is copied to SRAM by startup.S. Flash cannot be read
 * while the embedded flash controller is programming a page. */
__attribute__((section(".ramfunc"), noinline))
static uint32_t flash_program_page(uint32_t address, const uint8_t *bytes)
{
    volatile uint32_t *destination = (volatile uint32_t *)address;
    const uint32_t *source = (const uint32_t *)bytes;
    uint32_t page = (address - VOID_FLASH_BASE) / VOID_PAGE_SIZE;
    uint32_t status;

    while (!(MC_FSR & MC_FRDY)) { }
    for (unsigned i = 0; i < VOID_PAGE_SIZE / 4u; ++i)
        destination[i] = source[i];
    MC_FCR = MC_KEY | (page << 8) | MC_FCMD_WP;
    do { status = MC_FSR; } while (!(status & MC_FRDY));
    return status & (MC_LOCKE | MC_PROGE);
}

static int image_valid(uint32_t base, uint32_t size, uint32_t magic)
{
    const struct void_image_header *header =
        (const struct void_image_header *)base;
    uint32_t maximum = size - VOID_HEADER_SIZE;
    if (header->magic != magic || header->format != VOID_IMAGE_FORMAT)
        return 0;
    if (!header->body_length || header->body_length > maximum)
        return 0;
    if (header->entry < base + VOID_HEADER_SIZE ||
        header->entry >= base + VOID_HEADER_SIZE + header->body_length ||
        (header->entry & 3u))
        return 0;
    return crc32_memory((const uint8_t *)(base + VOID_HEADER_SIZE),
                        header->body_length) == header->body_crc32;
}

__attribute__((noreturn)) static void jump_to(uint32_t address)
{
    PIO_SODR = USB_PULLUP;
    UDP_TXVC = (1u << 8); /* Disable transceiver. */
    __asm__ volatile ("mov pc, %0\n" :: "r"(address) : "memory");
    for (;;) { }
}

__attribute__((noreturn)) static void jump_to_warm_recovery(void)
{
    /* This board's D+ pull-up cannot be disconnected reliably in software.
     * Keep the configured USB link and let stage 0 adopt its endpoints. */
    WARM_RECOVERY = WARM_RECOVERY_MAGIC;
    __asm__ volatile ("mov pc, %0\n" :: "r"(VOID_RECOVERY_BASE) : "memory");
    for (;;) { }
}

__attribute__((noreturn)) static void jump_to_warm_boot(void)
{
    WARM_RECOVERY = WARM_BOOT_MAGIC;
    __asm__ volatile ("mov pc, %0\n" :: "r"(VOID_BOOT_BASE + VOID_HEADER_SIZE) : "memory");
    for (;;) { }
}

static uint8_t choose_target(uint8_t target)
{
#if VOID_STAGE == 0
    if (target != 1) return ST_BAD_TARGET;
    update.base = VOID_BOOT_BASE;
    update.partition_size = VOID_BOOT_SIZE;
    update.magic = VOID_MAGIC_BOOT;
#else
    if (target != 2) return ST_BAD_TARGET;
    update.base = VOID_APP_BASE;
    update.partition_size = VOID_APP_SIZE;
    update.magic = VOID_MAGIC_APP;
#endif
    return ST_OK;
}

static uint8_t update_begin(const uint8_t *report)
{
    uint8_t result = choose_target(report[1]);
    if (result != ST_OK) return result;
    update.length = read_u32(report + 4);
    update.expected_crc = read_u32(report + 8);
    update.version = read_u32(report + 12);
    if (!update.length || update.length > update.partition_size - VOID_HEADER_SIZE)
        return ST_BAD_LENGTH;

    for (unsigned i = 0; i < VOID_PAGE_SIZE; ++i) update.page[i] = 0xFF;
    /* Invalidate the old header first. A power loss from this point leaves the
     * previous stage in recovery instead of executing a partial image. */
    if (flash_program_page(update.base, update.page)) return ST_FLASH;
    update.received = 0;
    update.page_address = update.base + VOID_HEADER_SIZE;
    update.page_used = 0;
    update.running_crc = 0xFFFFFFFFu;
    update.active = 1;
    return ST_OK;
}

static uint8_t flush_update_page(void)
{
    if (!update.page_used) return ST_OK;
    for (unsigned i = update.page_used; i < VOID_PAGE_SIZE; ++i)
        update.page[i] = 0xFF;
    if (flash_program_page(update.page_address, update.page)) {
        update.active = 0;
        return ST_FLASH;
    }
    update.page_address += VOID_PAGE_SIZE;
    update.page_used = 0;
    return ST_OK;
}

static uint8_t update_data(const uint8_t *report)
{
    uint8_t count = report[1];
    uint32_t offset = read_u32(report + 4);
    if (!update.active) return ST_BAD_STATE;
    if (!count || count > 56u || offset != update.received ||
        update.received + count > update.length)
        return ST_BAD_OFFSET;

    for (unsigned i = 0; i < count; ++i) {
        uint8_t value = report[8 + i];
        update.page[update.page_used++] = value;
        update.running_crc = crc32_byte(update.running_crc, value);
        update.received++;
        if (update.page_used == VOID_PAGE_SIZE) {
            uint8_t result = flush_update_page();
            if (result != ST_OK) return result;
        }
    }
    return ST_OK;
}

static uint8_t update_end(void)
{
    struct void_image_header *header = (struct void_image_header *)update.page;
    if (!update.active || update.received != update.length) return ST_BAD_STATE;
    if ((update.running_crc ^ 0xFFFFFFFFu) != update.expected_crc) {
        update.active = 0;
        return ST_CRC;
    }
    if (flush_update_page() != ST_OK) return ST_FLASH;

    for (unsigned i = 0; i < VOID_PAGE_SIZE; ++i) update.page[i] = 0xFF;
    header->magic = update.magic;
    header->format = VOID_IMAGE_FORMAT;
    header->body_length = update.length;
    header->body_crc32 = update.expected_crc;
    header->entry = update.base + VOID_HEADER_SIZE;
    header->version = update.version;
    if (flash_program_page(update.base, update.page)) {
        update.active = 0;
        return ST_FLASH;
    }
    update.active = 0;
    return image_valid(update.base, update.partition_size, update.magic) ?
           ST_OK : ST_BAD_IMAGE;
}

static void delay(volatile uint32_t count)
{
    while (count--) __asm__ volatile ("nop");
}

#if VOID_STAGE == 1
struct lcd_glyph { char character; uint8_t columns[5]; };

static const struct lcd_glyph lcd_font[] = {
    {'V',{0x1F,0x20,0x40,0x20,0x1F}}, {'O',{0x3E,0x41,0x41,0x41,0x3E}},
    {'I',{0x00,0x41,0x7F,0x41,0x00}}, {'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'b',{0x7F,0x48,0x44,0x44,0x38}}, {'o',{0x38,0x44,0x44,0x44,0x38}},
    {'t',{0x04,0x3F,0x44,0x40,0x20}}, {'l',{0x00,0x41,0x7F,0x40,0x00}},
    {'a',{0x20,0x54,0x54,0x54,0x78}}, {'d',{0x38,0x44,0x44,0x48,0x7F}},
    {'e',{0x38,0x54,0x54,0x54,0x18}}, {'r',{0x7C,0x08,0x04,0x04,0x08}},
    {'v',{0x1C,0x20,0x40,0x20,0x1C}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'.',{0x00,0x60,0x60,0x00,0x00}}
};

static void lcd_spi(uint8_t value)
{
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (value & 0x80u) PIO_SODR = LCD_MOSI;
        else PIO_CODR = LCD_MOSI;
        PIO_SODR = LCD_CLK;
        PIO_CODR = LCD_CLK;
        value <<= 1;
    }
}

static void lcd_command(uint8_t value)
{
    PIO_CODR = LCD_CS | LCD_A0;
    lcd_spi(value);
    PIO_SODR = LCD_CS;
}

static void lcd_data(uint8_t value)
{
    PIO_CODR = LCD_CS;
    PIO_SODR = LCD_A0;
    lcd_spi(value);
    PIO_SODR = LCD_CS;
}

static const uint8_t *lcd_glyph_for(char character)
{
    for (unsigned i = 0; i < sizeof(lcd_font) / sizeof(lcd_font[0]); ++i)
        if (lcd_font[i].character == character) return lcd_font[i].columns;
    return 0;
}

static void lcd_start_page(unsigned page)
{
    lcd_command((uint8_t)(0xB0u | page));
    lcd_command(0x10);
    lcd_command(0x00);
}

static void lcd_text(unsigned page, unsigned x, const char *text)
{
    lcd_start_page(page);
    unsigned written = 0;
    while (written++ < x) lcd_data(0);
    written = x;
    while (*text) {
        const uint8_t *glyph = lcd_glyph_for(*text++);
        for (unsigned column = 0; column < 5; ++column)
            lcd_data(glyph ? glyph[column] : 0);
        lcd_data(0);
        written += 6;
    }
    while (written++ < 132) lcd_data(0);
}

static void lcd_title(void)
{
    PIO_PER = LCD_MASK;
    PIO_OER = LCD_MASK;
    PIO_SODR = LCD_CS | LCD_RST;
    PIO_CODR = LCD_A0 | LCD_CLK | LCD_MOSI;
    PIO_CODR = LCD_RST;
    delay(2000);
    PIO_SODR = LCD_RST;
    delay(2000);
    lcd_command(0xAE); lcd_command(0xA2); lcd_command(0xA1);
    lcd_command(0xC0); lcd_command(0x40); lcd_command(0x2F);
    delay(10000);
    lcd_command(0x27); lcd_command(0x81); lcd_command(0x04);
    lcd_command(0xA6); lcd_command(0xA4); lcd_command(0xAF);
    for (unsigned page = 0; page < 8; ++page) lcd_text(page, 0, "");
    lcd_text(1, 54, "VOID");
    lcd_text(3, 36, "bootloader");
    lcd_text(5, 54, "v1.1");
}
#endif

/* CSR flags have mixed write-zero-to-clear and ordinary R/W semantics. */
static void csr_set(unsigned ep, uint32_t bits)
{
    uint32_t value = UDP_CSR(ep);
    value |= 0x4Fu;
    value &= ~0x30u;
    value |= bits;
    UDP_CSR(ep) = value;
    while ((UDP_CSR(ep) & bits) != bits) { }
}

static void csr_clear(unsigned ep, uint32_t bits)
{
    uint32_t value = UDP_CSR(ep);
    value |= 0x4Fu;
    value &= ~0x30u;
    value &= ~bits;
    UDP_CSR(ep) = value;
    while (UDP_CSR(ep) & bits) { }
}

static void ep0_send_next(void)
{
    uint16_t remaining = ep0_length - ep0_offset;
    uint16_t amount = remaining > EP0_SIZE ? EP0_SIZE : remaining;
    for (uint16_t i = 0; i < amount; ++i) {
        UDP_FDR(0) = ep0_data[ep0_offset++];
    }
    csr_set(0, UDP_TXPKTRDY);
}

static void ep0_send(const uint8_t *data, uint16_t length, uint16_t requested)
{
    if (length > requested) length = requested;
    ep0_data = data;
    ep0_length = length;
    ep0_offset = 0;
    ep0_zlp = (length < requested && (length % EP0_SIZE) == 0u);
    ep0_active = 1;
    ep0_send_next();
}

static void ep0_ack(void)
{
    ep0_active = 0;
    csr_set(0, UDP_TXPKTRDY);
}

static void ep0_stall(void)
{
    DEBUG_INC(6);
    csr_set(0, UDP_FORCESTALL);
}

static uint16_t u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void zero_report(void)
{
    for (unsigned i = 0; i < REPORT_SIZE; ++i) ep0_reply_buffer[i] = 0;
}

static void handle_setup(const uint8_t raw[8])
{
    struct setup_packet request;
    request.bmRequestType = raw[0];
    request.bRequest = raw[1];
    request.wValue = u16le(raw + 2);
    request.wIndex = u16le(raw + 4);
    request.wLength = u16le(raw + 6);

    /* GET_DESCRIPTOR, valid for device and HID interface recipients. */
    if ((request.bmRequestType & 0x80u) && request.bRequest == 6) {
        uint8_t type = (uint8_t)(request.wValue >> 8);
        uint8_t index = (uint8_t)request.wValue;
        if (type == 1) ep0_send(device_descriptor, sizeof(device_descriptor), request.wLength);
        else if (type == 2) ep0_send(configuration_descriptor, sizeof(configuration_descriptor), request.wLength);
        else if (type == 0x21) ep0_send(hid_descriptor, sizeof(hid_descriptor), request.wLength);
        else if (type == 0x22) ep0_send(hid_report_descriptor, sizeof(hid_report_descriptor), request.wLength);
        else if (type == 3 && index == 0) ep0_send(string_language, sizeof(string_language), request.wLength);
        else if (type == 3 && index == 1) ep0_send(string_manufacturer, sizeof(string_manufacturer), request.wLength);
        else if (type == 3 && index == 2) ep0_send(string_product, sizeof(string_product), request.wLength);
        else if (type == 3 && index == 3) ep0_send(string_serial, sizeof(string_serial), request.wLength);
        else if (type == 3 && index == 4) ep0_send(string_interface, sizeof(string_interface), request.wLength);
        else ep0_stall();
        return;
    }

    /* Standard device requests. */
    if ((request.bmRequestType & 0x60u) == 0) {
        if (request.bRequest == 0) { /* GET_STATUS */
            ep0_reply_buffer[0] = 0;
            ep0_reply_buffer[1] = 0;
            ep0_send(ep0_reply_buffer, 2, request.wLength);
        } else if (request.bRequest == 5) { /* SET_ADDRESS */
            pending_address = (uint8_t)(request.wValue & 0x7Fu);
            address_pending = 1;
            ep0_ack();
        } else if (request.bRequest == 8) { /* GET_CONFIGURATION */
            ep0_reply_buffer[0] = configured ? 1 : 0;
            ep0_send(ep0_reply_buffer, 1, request.wLength);
        } else if (request.bRequest == 9) { /* SET_CONFIGURATION */
            pending_configuration = (uint8_t)request.wValue;
            configuration_pending = 1;
            ep0_ack();
        } else if (request.bRequest == 10) { /* GET_INTERFACE */
            ep0_reply_buffer[0] = 0;
            ep0_send(ep0_reply_buffer, 1, request.wLength);
        } else if (request.bRequest == 11 || request.bRequest == 1 || request.bRequest == 3) {
            ep0_ack(); /* SET_INTERFACE / CLEAR_FEATURE / SET_FEATURE */
        } else {
            ep0_stall();
        }
        return;
    }

    /* Minimal HID class requests required by Windows. */
    if ((request.bmRequestType & 0x60u) == 0x20u) {
        if ((request.bmRequestType & 0x80u) && request.bRequest == 1) { /* GET_REPORT */
            zero_report();
            ep0_reply_buffer[0] = 0x56; /* V */
            ep0_reply_buffer[1] = 0x42; /* B */
            ep0_reply_buffer[2] = 1;    /* protocol version */
            ep0_send(ep0_reply_buffer, REPORT_SIZE, request.wLength);
        } else if ((request.bmRequestType & 0x80u) && request.bRequest == 2) { /* GET_IDLE */
            ep0_reply_buffer[0] = 0;
            ep0_send(ep0_reply_buffer, 1, request.wLength);
        } else if (!(request.bmRequestType & 0x80u) &&
                   (request.bRequest == 9 || request.bRequest == 10 || request.bRequest == 11)) {
            ep0_ack(); /* SET_REPORT/SET_IDLE/SET_PROTOCOL */
        } else {
            ep0_stall();
        }
        return;
    }

    ep0_stall();
}

static void reset_endpoints(void)
{
    DEBUG_INC(1);
    UDP_RST_EP = 0x07u;
    UDP_RST_EP = 0;
    UDP_FADDR = UDP_FEN;
    UDP_GLB_STAT = 0;
    UDP_CSR(0) = UDP_EPEDS | UDP_EPTYPE_CTRL;
    while (!(UDP_CSR(0) & UDP_EPEDS)) { }
    ep0_active = 0;
    address_pending = 0;
    configuration_pending = 0;
    configured = 0;
}

static void finish_control_ack(void)
{
    if (address_pending) {
        UDP_FADDR = UDP_FEN | pending_address;
        UDP_GLB_STAT = pending_address ? UDP_FADDEN : 0;
        address_pending = 0;
    }
    if (configuration_pending) {
        configured = pending_configuration ? 1 : 0;
        if (configured) {
            DEBUG_INC(7);
            UDP_GLB_STAT = UDP_FADDEN | UDP_CONFG;
            UDP_CSR(1) = UDP_EPEDS | UDP_EPTYPE_INT_IN;
            while (!(UDP_CSR(1) & UDP_EPEDS)) { }
            UDP_CSR(2) = UDP_EPEDS | UDP_EPTYPE_INT_OUT;
            while (!(UDP_CSR(2) & UDP_EPEDS)) { }
        } else {
            UDP_GLB_STAT = UDP_FADDEN;
        }
        configuration_pending = 0;
    }
}

static void poll_ep0(void)
{
    uint32_t csr = UDP_CSR(0);

    if (csr & UDP_RXSETUP) {
        uint8_t raw[8];
        unsigned count = UDP_RXBYTECNT(csr);
        for (unsigned i = 0; i < count; ++i) {
            uint8_t byte = (uint8_t)UDP_FDR(0);
            if (i < 8) raw[i] = byte;
        }
        DEBUG_INC(2);
        DEBUG_SET(3, (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                     ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24));
        DEBUG_SET(4, (uint32_t)raw[4] | ((uint32_t)raw[5] << 8) |
                     ((uint32_t)raw[6] << 16) | ((uint32_t)raw[7] << 24));
        if (count == 8 && (raw[0] & 0x80u)) csr_set(0, UDP_DIR);
        else csr_clear(0, UDP_DIR);
        csr_clear(0, UDP_RXSETUP);
        if (count == 8) handle_setup(raw);
        else ep0_stall();
        return;
    }

    if (csr & UDP_TXCOMP) {
        DEBUG_INC(5);
        csr_clear(0, UDP_TXCOMP);
        if (ep0_active) {
            if (ep0_offset < ep0_length) {
                ep0_send_next();
            } else if (ep0_zlp) {
                ep0_zlp = 0;
                csr_set(0, UDP_TXPKTRDY);
            } else {
                ep0_active = 0;
            }
        } else {
            finish_control_ack();
        }
    }

    csr = UDP_CSR(0);
    if (csr & UDP_RX_DATA_BK0) csr_clear(0, UDP_RX_DATA_BK0);
    if (csr & UDP_STALLSENT) csr_clear(0, UDP_STALLSENT);
}

static void process_command(void)
{
    for (unsigned i = 0; i < REPORT_SIZE; ++i) in_report[i] = 0;
    in_report[0] = out_report[0];
    in_report[1] = ST_OK;
    if (out_report[0] == CMD_INFO) {
        in_report[1] = 'V'; in_report[2] = 'O'; in_report[3] = 'I'; in_report[4] = 'D';
        in_report[5] = 1; /* protocol major */
        in_report[6] = 1; /* protocol minor */
        in_report[7] = VOID_STAGE;
        in_report[8] = 52; /* application KiB */
        in_report[9] = 1; /* transactional flash writes enabled */
        write_u32(in_report + 12, VOID_BOOT_BASE);
        write_u32(in_report + 16, VOID_BOOT_SIZE);
        write_u32(in_report + 20, VOID_APP_BASE);
        write_u32(in_report + 24, VOID_APP_SIZE);
    } else if (out_report[0] == CMD_BEGIN) {
        in_report[1] = update_begin(out_report);
    } else if (out_report[0] == CMD_DATA) {
        in_report[1] = update_data(out_report);
        write_u32(in_report + 4, update.received);
    } else if (out_report[0] == CMD_END) {
        in_report[1] = update_end();
    } else if (out_report[0] == CMD_ABORT) {
        update.active = 0;
    } else if (out_report[0] == CMD_BOOT) {
#if VOID_STAGE == 0
        if (image_valid(VOID_BOOT_BASE, VOID_BOOT_SIZE, VOID_MAGIC_BOOT)) deferred_action = 1;
        else in_report[1] = ST_BAD_IMAGE;
#else
        if (image_valid(VOID_APP_BASE, VOID_APP_SIZE, VOID_MAGIC_APP)) deferred_action = 2;
        else in_report[1] = ST_BAD_IMAGE;
#endif
    } else if (out_report[0] == CMD_RECOVERY) {
#if VOID_STAGE == 1
        deferred_action = 3;
#else
        in_report[1] = ST_BAD_COMMAND;
#endif
    } else {
        in_report[1] = ST_BAD_COMMAND;
    }
    in_report_pending = 1;
}

static void poll_data_endpoints(void)
{
    uint32_t csr2 = UDP_CSR(2);
    uint32_t bank = csr2 & UDP_RX_DATA_BK0 ? UDP_RX_DATA_BK0 :
                    (csr2 & UDP_RX_DATA_BK1 ? UDP_RX_DATA_BK1 : 0u);
    if (bank) {
        unsigned count = UDP_RXBYTECNT(csr2);
        for (unsigned i = 0; i < count; ++i) {
            uint8_t value = (uint8_t)UDP_FDR(2);
            if (i < REPORT_SIZE) out_report[i] = value;
        }
        csr_clear(2, bank);
        if (count == REPORT_SIZE) process_command();
    }

    if ((UDP_CSR(1) & UDP_TXCOMP) != 0) {
        csr_clear(1, UDP_TXCOMP);
        if (deferred_action == 1)
            jump_to_warm_boot();
        if (deferred_action == 2)
            jump_to(((const struct void_image_header *)VOID_APP_BASE)->entry);
        if (deferred_action == 3)
            jump_to_warm_recovery();
    }
    if (in_report_pending && !(UDP_CSR(1) & UDP_TXPKTRDY)) {
        for (unsigned i = 0; i < REPORT_SIZE; ++i) UDP_FDR(1) = in_report[i];
        csr_set(1, UDP_TXPKTRDY);
        in_report_pending = 0;
    }
}

static void clocks_init(void)
{
    WDT_MR = 0x00008000u;
    MC_FMR = 0x00490100u;
    PMC_MOR = 0x00000601u;
    while (!(PMC_SR & (1u << 0))) { }
    PMC_PLLR = 0x10481C0Eu; /* 96MHz PLL, USB divided to 48MHz. */
    while (!(PMC_SR & (1u << 2))) { }
    PMC_MCKR = 0x00000007u; /* PLL / 2 = approximately 48MHz MCK. */
    while (!(PMC_SR & (1u << 3))) { }
}

static void usb_init(void)
{
    /* Peripheral clocks: PIOA ID2 and UDP ID11; system USB clock bit 7. */
    PMC_PCER = (1u << 2) | (1u << 11);
    PMC_SCER = (1u << 7);

    /* Disconnect from the host while the controller is reset. */
    PIO_PER = USB_PULLUP;
    PIO_OER = USB_PULLUP;
    PIO_SODR = USB_PULLUP;
    delay(200000u);

    UDP_IDR = 0xFFFFFFFFu;
    UDP_ICR = 0xFFFFFFFFu;
    UDP_TXVC = 0; /* Enable the USB transceiver. */
    reset_endpoints();

    /* Reference SAM7S designs use an active-low PA16 pull-up switch. */
    PIO_CODR = USB_PULLUP;
}

int main(void)
{
#if VOID_STAGE == 0
    uint8_t warm_recovery = 0;
    if (WARM_RECOVERY == WARM_RECOVERY_MAGIC) {
        WARM_RECOVERY = 0;
        warm_recovery = 1;
    }
#endif
#if VOID_STAGE == 1
    uint8_t warm_boot = 0;
    if (WARM_RECOVERY == WARM_BOOT_MAGIC) {
        WARM_RECOVERY = 0;
        warm_boot = 1;
    }
#endif
    clocks_init();
    PMC_PCER = (1u << 2);
    PIO_PER = BUTTON_GREEN | BUTTON_RED;
    PIO_ODR = BUTTON_GREEN | BUTTON_RED;
    PIO_PUER = BUTTON_GREEN | BUTTON_RED;
    delay(10000u);

#if VOID_STAGE == 0
    if (!warm_recovery &&
        (PIO_PDSR & (BUTTON_GREEN | BUTTON_RED)) != 0 &&
        image_valid(VOID_BOOT_BASE, VOID_BOOT_SIZE, VOID_MAGIC_BOOT))
        jump_to(((const struct void_image_header *)VOID_BOOT_BASE)->entry);
#else
    if (!warm_boot && (PIO_PDSR & BUTTON_GREEN) != 0 &&
        image_valid(VOID_APP_BASE, VOID_APP_SIZE, VOID_MAGIC_APP))
        jump_to(((const struct void_image_header *)VOID_APP_BASE)->entry);
#endif

    DEBUG_SET(0, 0x56424944u); /* "VBID" */
#if VOID_STAGE == 0
    if (warm_recovery) {
        configured = 1;
        DEBUG_SET(7, 1);
    } else {
        usb_init();
    }
#else
    if (warm_boot) configured = 1;
    else usb_init();
    lcd_title();
#endif
    DEBUG_SET(9, 0x56424944u);

    for (;;) {
        uint32_t isr = UDP_ISR;
        if (isr & UDP_ENDBUSRES) {
            UDP_ICR = UDP_ENDBUSRES;
            reset_endpoints();
        }
        poll_ep0();
        if (configured) poll_data_endpoints();
    }
}

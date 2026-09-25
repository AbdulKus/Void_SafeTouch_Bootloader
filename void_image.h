#ifndef VOID_IMAGE_H
#define VOID_IMAGE_H

#include <stdint.h>

#define VOID_FLASH_BASE       0x00100000u
#define VOID_RECOVERY_BASE    0x00100000u
#define VOID_RECOVERY_SIZE    0x00001000u
#define VOID_BOOT_BASE        0x00101000u
#define VOID_BOOT_SIZE        0x00002000u
#define VOID_APP_BASE         0x00103000u
#define VOID_APP_SIZE         0x0000D000u
#define VOID_HEADER_SIZE      128u
#define VOID_PAGE_SIZE        128u

#define VOID_MAGIC_BOOT       0x31425256u /* "VRB1" little endian */
#define VOID_MAGIC_APP        0x31504156u /* "VAP1" little endian */
#define VOID_IMAGE_FORMAT     1u

struct void_image_header {
    uint32_t magic;
    uint32_t format;
    uint32_t body_length;
    uint32_t body_crc32;
    uint32_t entry;
    uint32_t version;
    uint32_t reserved[26];
};

typedef char void_header_must_be_one_page[
    sizeof(struct void_image_header) == VOID_HEADER_SIZE ? 1 : -1];

#endif

#ifndef SAFETOUCH_SHA256_H
#define SAFETOUCH_SHA256_H

#include <stdint.h>

struct sha256_context {
    uint32_t state[8];
    uint64_t bits;
    uint8_t block[64];
    unsigned used;
};

void sha256_init(struct sha256_context *context);
void sha256_update(struct sha256_context *context, const void *data, unsigned length);
void sha256_final(struct sha256_context *context, uint8_t digest[32]);
void sha256(const void *data, unsigned length, uint8_t digest[32]);
void hmac_sha256(const uint8_t *key, unsigned key_length,
                 const void *data, unsigned data_length, uint8_t digest[32]);

#endif

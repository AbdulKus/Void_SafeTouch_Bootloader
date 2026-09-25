#include "sha256.h"

static const uint32_t constants[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t rotate(uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32u - count));
}

static void transform(struct sha256_context *context, const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a,b,c,d,e,f,g,h;
    for (unsigned i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
    for (unsigned i = 16; i < 64; ++i) {
        uint32_t s0 = rotate(w[i-15],7) ^ rotate(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotate(w[i-2],17) ^ rotate(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=context->state[0]; b=context->state[1]; c=context->state[2]; d=context->state[3];
    e=context->state[4]; f=context->state[5]; g=context->state[6]; h=context->state[7];
    for (unsigned i = 0; i < 64; ++i) {
        uint32_t s1 = rotate(e,6) ^ rotate(e,11) ^ rotate(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + constants[i] + w[i];
        uint32_t s0 = rotate(a,2) ^ rotate(a,13) ^ rotate(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    context->state[0]+=a; context->state[1]+=b; context->state[2]+=c; context->state[3]+=d;
    context->state[4]+=e; context->state[5]+=f; context->state[6]+=g; context->state[7]+=h;
}

void sha256_init(struct sha256_context *context)
{
    static const uint32_t initial[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };
    for (unsigned i = 0; i < 8; ++i) context->state[i] = initial[i];
    context->bits = 0;
    context->used = 0;
}

void sha256_update(struct sha256_context *context, const void *input, unsigned length)
{
    const uint8_t *data = (const uint8_t *)input;
    context->bits += (uint64_t)length * 8u;
    while (length) {
        unsigned amount = 64u - context->used;
        if (amount > length) amount = length;
        for (unsigned i = 0; i < amount; ++i) context->block[context->used+i] = data[i];
        context->used += amount; data += amount; length -= amount;
        if (context->used == 64u) { transform(context, context->block); context->used = 0; }
    }
}

void sha256_final(struct sha256_context *context, uint8_t digest[32])
{
    uint64_t bits = context->bits;
    context->block[context->used++] = 0x80;
    if (context->used > 56u) {
        while (context->used < 64u) context->block[context->used++] = 0;
        transform(context, context->block); context->used = 0;
    }
    while (context->used < 56u) context->block[context->used++] = 0;
    for (unsigned i = 0; i < 8; ++i) context->block[63u-i] = (uint8_t)(bits >> (i*8u));
    transform(context, context->block);
    for (unsigned i = 0; i < 8; ++i) {
        digest[i*4] = (uint8_t)(context->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(context->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(context->state[i] >> 8);
        digest[i*4+3] = (uint8_t)context->state[i];
    }
    for (unsigned i = 0; i < sizeof(*context); ++i) ((volatile uint8_t *)context)[i] = 0;
}

void sha256(const void *data, unsigned length, uint8_t digest[32])
{
    struct sha256_context context;
    sha256_init(&context); sha256_update(&context, data, length); sha256_final(&context, digest);
}

void hmac_sha256(const uint8_t *key, unsigned key_length,
                 const void *data, unsigned data_length, uint8_t digest[32])
{
    uint8_t padded[64], inner[32];
    struct sha256_context context;
    if (key_length > 64u) { sha256(key, key_length, inner); key = inner; key_length = 32u; }
    for (unsigned i = 0; i < 64; ++i) padded[i] = (uint8_t)((i < key_length ? key[i] : 0u) ^ 0x36u);
    sha256_init(&context); sha256_update(&context, padded, 64); sha256_update(&context, data, data_length); sha256_final(&context, inner);
    for (unsigned i = 0; i < 64; ++i) padded[i] = (uint8_t)((i < key_length ? key[i] : 0u) ^ 0x5cu);
    sha256_init(&context); sha256_update(&context, padded, 64); sha256_update(&context, inner, 32); sha256_final(&context, digest);
    for (unsigned i = 0; i < 64; ++i) ((volatile uint8_t *)padded)[i] = 0;
    for (unsigned i = 0; i < 32; ++i) ((volatile uint8_t *)inner)[i] = 0;
}

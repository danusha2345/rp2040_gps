#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

// Глобальные объявления
extern struct SHA256_CTX *ctx;
extern uint8_t hash[32];

// Структура контекста
typedef struct SHA256_CTX {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

// Прототипы функций
void sha256_init();
void sha256_update(const uint8_t *data, size_t len);
void sha256_final();
char* bytes_to_hex(const uint8_t *bytes, size_t len);
void sha256_cleanup();

#endif
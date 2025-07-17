/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */
 
#ifndef HASH_FUNC_H
#define HASH_FUNC_H
#include <stddef.h>
#include <stdint.h>

static uint32_t hash_djb2(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

uint32_t hash_key(const char* key, size_t hash_table_capacity);
void sha256_string(const char *str, char *output);

#endif //HASH_FUNC_H

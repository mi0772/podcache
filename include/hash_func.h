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



uint32_t hash_key(const char* key, size_t hash_table_capacity);
uint32_t hash(const char* key);
void sha256_string(const char *str, char *output);

#endif //HASH_FUNC_H

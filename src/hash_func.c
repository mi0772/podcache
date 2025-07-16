/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/hash_func.h"

uint32_t hash_key(const char* key, size_t hash_table_capacity) {
    return hash_djb2(key) % hash_table_capacity;
}
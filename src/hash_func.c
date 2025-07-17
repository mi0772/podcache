/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/hash_func.h"
#include <openssl/sha.h>
#include <string.h>

#include <stdio.h>

uint32_t hash_key(const char* key, size_t hash_table_capacity) {
    return hash_djb2(key) % hash_table_capacity;
}

void sha256_string(const char* str, char* output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;

    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str, strlen(str));
    SHA256_Final(hash, &sha256);

    // Converti in stringa esadecimale
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}
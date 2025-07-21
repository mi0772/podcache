/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/hash_func.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>


static uint32_t hash_djb2(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

uint32_t hash_key(const char* key, size_t hash_table_capacity) {
    return hash(key) % hash_table_capacity;
}

uint32_t hash(const char* key) {
    return hash_djb2(key);
}


void sha256_string(const char* str, char* output) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        // Gestione errore
        output[0] = '\0';
        return;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, str, strlen(str)) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        // Gestione errore
        EVP_MD_CTX_free(ctx);
        output[0] = '\0';
        return;
        }

    EVP_MD_CTX_free(ctx);

    // Converti in stringa esadecimale
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[hash_len * 2] = '\0';
}


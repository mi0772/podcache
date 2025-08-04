/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 04/08/25
 * License: AGPL 3
 */

#include "../include/hash_func.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SHA-256 implementation without OpenSSL dependency
// Based on FIPS 180-4 specification

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }

static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }

static uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }

static uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }

static uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }

static uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

static uint32_t hash_djb2(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

uint32_t hash_key(const char *key, size_t hash_table_capacity) {
    return hash(key) % hash_table_capacity;
}

uint32_t hash(const char *key) { return hash_djb2(key); }

void sha256_string(const char *str, char *output) {
    size_t len = strlen(str);

    // Initial hash values (first 32 bits of the fractional parts of the square roots of the first 8
    // primes)
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    // Pre-processing: adding padding bits
    size_t msg_len = len;
    size_t bit_len = msg_len * 8;

    // Calculate total length after padding
    size_t total_len = msg_len + 1; // +1 for the '1' bit
    while (total_len % 64 != 56) {
        total_len++;
    }
    total_len += 8; // +8 for the length field

    // Create padded message
    uint8_t *msg = (uint8_t *)calloc(total_len, 1);
    if (!msg) {
        output[0] = '\0';
        return;
    }

    memcpy(msg, str, msg_len);
    msg[msg_len] = 0x80; // Append '1' bit

    // Append length as 64-bit big-endian
    for (int i = 0; i < 8; i++) {
        msg[total_len - 1 - i] = (bit_len >> (i * 8)) & 0xff;
    }

    // Process message in 512-bit chunks
    for (size_t chunk = 0; chunk < total_len; chunk += 64) {
        uint32_t w[64] = {0};

        // Copy chunk into first 16 words of the message schedule array w
        for (int i = 0; i < 16; i++) {
            w[i] = (msg[chunk + i * 4] << 24) | (msg[chunk + i * 4 + 1] << 16) |
                   (msg[chunk + i * 4 + 2] << 8) | (msg[chunk + i * 4 + 3]);
        }

        // Extend the first 16 words into the remaining 48 words
        for (int i = 16; i < 64; i++) {
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
        }

        // Initialize working variables
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], h_temp = h[7];

        // Compression function main loop
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h_temp + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            h_temp = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        // Add compressed chunk to current hash value
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += h_temp;
    }

    // Convert hash to hex string
    for (int i = 0; i < 8; i++) {
        sprintf(output + (i * 8), "%08x", h[i]);
    }
    output[64] = '\0';

    free(msg);
}

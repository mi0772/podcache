/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 25/07/25
 * License: MIT
 */

#include "b64.h"

#include <stdlib.h>
#include <string.h>

int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -1; // padding
    return -2; // carattere invalido
}

char *base64_decode(const char *input, size_t *output_len) {
    if (!input || !output_len) return NULL;

    size_t input_len = strlen(input);
    if (input_len == 0) {
        *output_len = 0;
        char *result = malloc(1);
        if (result) result[0] = '\0';
        return result;
    }

    // Rimuovi padding per calcolare la lunghezza output
    size_t padding = 0;
    if (input_len >= 2) {
        if (input[input_len - 1] == '=') padding++;
        if (input[input_len - 2] == '=') padding++;
    }

    *output_len = (input_len * 3) / 4 - padding;
    char *output = malloc(*output_len + 1); // +1 per null terminator
    if (!output) return NULL;

    size_t out_pos = 0;
    for (size_t i = 0; i < input_len; i += 4) {
        int b[4];
        for (int j = 0; j < 4; j++) {
            if (i + j < input_len) {
                b[j] = base64_decode_char(input[i + j]);
                if (b[j] == -2) { // carattere invalido
                    free(output);
                    return NULL;
                }
            } else {
                b[j] = 0;
            }
        }

        if (out_pos < *output_len) {
            output[out_pos++] = (b[0] << 2) | (b[1] >> 4);
        }
        if (out_pos < *output_len && b[2] != -1) {
            output[out_pos++] = ((b[1] & 0x0F) << 4) | (b[2] >> 2);
        }
        if (out_pos < *output_len && b[3] != -1) {
            output[out_pos++] = ((b[2] & 0x03) << 6) | b[3];
        }
    }

    output[*output_len] = '\0'; // null terminator per sicurezza
    return output;
}

char *base64_encode(const char *input, size_t input_len) {
    if (!input || input_len == 0) return NULL;

    size_t output_len = ((input_len + 2) / 3) * 4;
    char *output = malloc(output_len + 1);
    if (!output) return NULL;

    size_t out_pos = 0;
    for (size_t i = 0; i < input_len; i += 3) {
        int b1 = input[i];
        int b2 = (i + 1 < input_len) ? input[i + 1] : 0;
        int b3 = (i + 2 < input_len) ? input[i + 2] : 0;

        output[out_pos++] = base64_chars[b1 >> 2];
        output[out_pos++] = base64_chars[((b1 & 0x03) << 4) | (b2 >> 4)];
        output[out_pos++] = (i + 1 < input_len) ? base64_chars[((b2 & 0x0F) << 2) | (b3 >> 6)] : '=';
        output[out_pos++] = (i + 2 < input_len) ? base64_chars[b3 & 0x3F] : '=';
    }

    output[output_len] = '\0';
    return output;
}
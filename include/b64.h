/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 25/07/25
 * License: MIT
 */
 
#ifndef B64_H
#define B64_H
#include <stdio.h>

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_decode_char(char c);
char *base64_decode(const char *input, size_t *output_len);
char *base64_encode(const char *input, size_t input_len);


#endif //B64_H

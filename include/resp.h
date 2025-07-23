/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 22/07/25
 * License: MIT
 */
 
#ifndef RESP_H
#define RESP_H
#include <openssl/bio.h>

typedef enum resp_type {
    RESP_ARRAY,
    RESP_ERROR,
    RESP_LONG,
    RESP_BULK_STRING,
    RESP_SIMPLE_STRING,
    RESP_UNKNOW
} resp_type_t;

typedef struct {
    char *command;
    char *current_position;
    size_t command_length;
} resp_command_raw_t;

resp_command_raw_t resp_command_create(const char *command_raw);

#endif //RESP_H

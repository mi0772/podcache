/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 26/07/25
 * License: MIT
 */
 
#ifndef SERVER_COMMAND_H
#define SERVER_COMMAND_H
#include <stdio.h>

typedef enum command_type {
    CMD_PING,
    CMD_PUT,
    CMD_GET,
    CMD_DEL,
    CMD_STAT,
    CMD_QUIT,
    CMD_UNKNOWN
} command_type_e;

typedef struct {
    char *user_command;
    char *key;
    char *value;
    size_t key_size;
    size_t value_size;
} command_t;

#endif //SERVER_COMMAND_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 22/07/25
 * License: MIT
 */
#include "resp.h"

#include <string.h>

resp_type_t get_type(char c) {
    switch (c) {
        case '*': return RESP_ARRAY;
        case '$': return RESP_BULK_STRING;
        case '+': return RESP_SIMPLE_STRING;
        case '-': return RESP_ERROR;
        case ':': return RESP_LONG;
        default: return RESP_UNKNOW;
    }
}

resp_command_raw_t resp_command_create(const char *command_raw) {
    resp_command_raw_t resp_command = {};
    resp_command.command = strdup(command_raw);
    resp_command.current_position = resp_command.command;
    resp_command.command_length = strlen(command_raw);
    return resp_command;
}
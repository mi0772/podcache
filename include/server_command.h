/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 25/07/25
 * License: MIT
 */
 
#ifndef SERVER_COMMAND_H
#define SERVER_COMMAND_H
#include <stdio.h>

static const char* command_names[] = {
    "PING",
    "PUT",
    "GET",
    "QUIT",
    "INFO",
    "REM"
};

typedef enum {
    CMD_PING = 0,
    CMD_PUT,
    CMD_GET,
    CMD_QUIT,
    CMD_INFO,
    CMD_REMOVE,
    CMD_UNKNOWN
} command_type_t;

typedef struct {
    command_type_t command;
    char *key;          // Decodificato da base64
    char *value;        // Decodificato da base64
    size_t key_len;     // Lunghezza dopo decodifica
    size_t value_len;   // Lunghezza dopo decodifica
} parsed_command_t;

command_type_t parse_command_name(const char *cmd_str);
parsed_command_t *parse_command(char *raw_command);
void free_parsed_command(parsed_command_t *cmd);
void print_parsed_command(const parsed_command_t *cmd);


#endif //SERVER_COMMAND_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 28/07/25
 * License: AGPL 3
 */
 
#ifndef RESP_PARSER_H
#define RESP_PARSER_H
#include <stddef.h>

#define MAX_ARGS 100
#define MAX_STR_LEN (1024 * 1024)
#define MIN_BUFFER_SIZE 4


typedef struct {
    const char *data;
    size_t len;
    size_t pos;
} buffer_t;

// Risultati di parsing
typedef enum {
    PARSE_OK = 1,
    PARSE_INCOMPLETE = 0,
    PARSE_ERROR = -1
} parse_result_t;

typedef enum {
    RESP_SET,
    RESP_GET,
    RESP_DEL,
    RESP_PING,
    RESP_QUIT,
    RESP_CLIENT,
    RESP_UNKNOW,
    RESP_INCR,
    RESP_UNLINK
} resp_command_e;

typedef struct {
    char *command; // resp command
    char **args;   // argument list
    int arg_count; // number of arguments
} resp_command_t;

resp_command_e decode_command(char *command);
int resp_parse(const char *buf, size_t buf_len, resp_command_t *out);
void resp_command_free(resp_command_t *cmd);
resp_command_e resp_decode_command(const char *command);

#endif //RESP_PARSER_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 26/07/25
 * License: MIT
 */
 
#ifndef RESP_H
#define RESP_H
#include <stdbool.h>

typedef enum {
    RESP_SIMPLE_STRING,
    RESP_SIMPLE_ERROR,
    RESP_INTEGER,
    RESP_BULK_STRING,
    RESP_ARRAY,
    RESP_NULL,
    RESP_BOOL,
    RESP_DOUBLE,
    RESP_BIG_NUMBER
} resp_type_e;

typedef struct {
    char *command; // command received from resp
    char *command_pos; // pointer to current command position
} resp_command_t;

typedef struct {
    void *value;
    bool success;
} parse_result_t;

char resp_first_byte(resp_type_e resp_type);
resp_type_e resp_command_type(char p);
void parse_resp_command(resp_command_t *resp_command);

#endif //RESP_H

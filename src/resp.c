/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 26/07/25
 * License: MIT
 */

#include "../include/resp.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char resp_first_byte(resp_type_e resp_type) {
    switch (resp_type) {
        case RESP_SIMPLE_STRING:    return '+';
        case RESP_SIMPLE_ERROR:     return '-';
        case RESP_INTEGER:          return ':';
        case RESP_BULK_STRING:      return '$';
        case RESP_ARRAY:            return '*';
        case RESP_NULL:             return '_';
        case RESP_BOOL:             return '#';
        case RESP_DOUBLE:           return ',';
        case RESP_BIG_NUMBER:       return '(';
    }
    return '\0';
}

resp_type_e resp_command_type(char p) {
    switch (p) {
        case '+': return RESP_SIMPLE_STRING;
        case '-': return RESP_SIMPLE_ERROR;
        case ':': return RESP_INTEGER;
        case '$': return RESP_BULK_STRING;
        case '*': return RESP_ARRAY;
        case '_': return RESP_NULL;
        case '#': return RESP_BOOL;
        case ',': return RESP_DOUBLE;
        case '(': return RESP_BIG_NUMBER;
        default : return -1;
    }
}

// Trova il prossimo separatore \r\n
static char *next_separator(resp_command_t *resp_command) {
    char *result = resp_command->command_pos;

    while (*result != '\0') {
        if (*result == '\r' && *(result+1) == '\n') {
            return result;
        }
        result++;
    }
    return NULL;
}

static char *extract_token_until_separator(resp_command_t *resp_command) {
    char *sep_start_pos = next_separator(resp_command);
    if (sep_start_pos == NULL) {
        return NULL; // Separatore non trovato
    }

    size_t result_len = sep_start_pos - resp_command->command_pos;

    char *result = malloc(result_len + 1);
    if (result == NULL) {
        return NULL; // Errore di allocazione
    }

    strncpy(result, resp_command->command_pos, result_len);
    result[result_len] = '\0';

    resp_command->command_pos = sep_start_pos + 2; // Salta \r\n

    return result;
}

char *parse_simple_string(resp_command_t *resp_command) {
    resp_command->command_pos++; // Salta il carattere identificativo
    return extract_token_until_separator(resp_command);
}


parse_result_t parse_integer(resp_command_t *resp_command) {
    parse_result_t result = {0, false};
    resp_command->command_pos++;

    int sign = 1;
    if (*resp_command->command_pos == '+' || *resp_command->command_pos == '-') {
        if (*resp_command->command_pos == '-') {
            sign = -1;
        }
        resp_command->command_pos++;
    }

    char *token = extract_token_until_separator(resp_command);
    if (token == NULL) return result;

    char *endptr;
    long parsed_value = strtol(token, &endptr, 10);

    if (*endptr == '\0' && parsed_value <= INT_MAX && parsed_value >= INT_MIN) {
        result.value = (int *)(parsed_value * sign);
        result.success = true;
    }

    free(token);
    return result;
}

parse_result_t parse_bulk_string(resp_command_t *resp_command) {
    parse_result_t result = {
        .value = NULL,
        .success = false
    };
    resp_command->command_pos++; //salto il carattere $
    //ora mi serve la lunghezza della stringa
    char *len_token = extract_token_until_separator(resp_command);
    if (len_token == NULL) return result;
    char *endptr;
    long parsed_len = strtol(len_token, &endptr, 10);

    char *token = extract_token_until_separator(resp_command);
    if (token == NULL) return result;

    if (strlen(token) != parsed_len) {
        free(token);
        free(len_token);
        return result;
    }
    result.success = true;
    result.value = token;
    free(len_token);
    return result;
}

void parse_resp_command(resp_command_t *resp_command) {
    resp_type_e type = resp_command_type(resp_command->command[0]);

    switch (type) {
        case RESP_SIMPLE_STRING:
        case RESP_SIMPLE_ERROR: {
            printf("command is simple string or simple error\n");
            char *result = parse_simple_string(resp_command);
            if (result != NULL) {
                printf("result of parse simple string or error is: %s\n", result);
                free(result);
            } else {
                printf("Error parsing simple string/error\n");
            }
            break;
        }
        case RESP_INTEGER: {
            printf("command is integer\n");
            parse_result_t result = parse_integer(resp_command);
            if (result.success) {
                printf("result of parse integer is: %d\n", (int)result.value);
            } else {
                printf("Error parsing integer\n");
            }
            break;
        }
        case RESP_BULK_STRING: {
            printf("command is bulk string\n");
            parse_result_t result = parse_bulk_string(resp_command);
            if (result.success) {
                printf("result of parse bulk string is: %s\n", (char *)result.value);
            } else {
                printf("Error parsing bulk string\n");
            }
            break;
        }
        default: {
            printf("unknown command type\n");
            break;
        }
    }
}
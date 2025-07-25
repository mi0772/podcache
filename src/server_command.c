/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 25/07/25
 * License: MIT
 */

#include "../include/server_command.h"

#include <_strings.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "b64.h"

#define DELIMITER ":"

command_type_t parse_command_name(const char *cmd_str) {
    if (!cmd_str) return CMD_UNKNOWN;

    for (int i = 0; i < sizeof(command_names)/sizeof(command_names[0]); i++) {
        if (strcasecmp(cmd_str, command_names[i]) == 0) {
            return (command_type_t)i;
        }
    }
    return CMD_UNKNOWN;
}

parsed_command_t *parse_command(char *raw_command) {
    if (!raw_command) return NULL;

    parsed_command_t *cmd = calloc(1, sizeof(parsed_command_t));
    if (!cmd) return NULL;

    // 1. Prima strtok per il comando
    char *cmd_str = strtok(raw_command, DELIMITER);
    if (!cmd_str) {
        free(cmd);
        return NULL;
    }

    cmd->command = parse_command_name(cmd_str);  // Passa il token, non la stringa originale

    if (cmd->command == CMD_UNKNOWN) {
        free(cmd);
        return NULL;
    }

    // 2. Controlla se il comando richiede parametri
    bool needs_key = (cmd->command == CMD_PUT || cmd->command == CMD_GET);
    bool needs_value = (cmd->command == CMD_PUT);

    // 3. Estrai la key se necessaria
    if (needs_key) {
        char *key_b64 = strtok(NULL, DELIMITER);  // Usa DELIMITER, non ":"
        if (key_b64) {
            cmd->key = base64_decode(key_b64, &cmd->key_len);

            if (!cmd->key) {
                free(cmd);
                return NULL;
            }
        }
    }

    // 4. Estrai il value se necessario
    if (needs_value) {
        char *value_b64 = strtok(NULL, DELIMITER);  // Usa DELIMITER, non ":"
        if (value_b64) {
            cmd->value = base64_decode(value_b64, &cmd->value_len);

            if (!cmd->value) {
                if (cmd->key) free(cmd->key);
                free(cmd);
                return NULL;
            }
        }
    }

    return cmd;
}

// Cleanup
void free_parsed_command(parsed_command_t *cmd) {
    if (cmd) {
        if (cmd->key) free(cmd->key);
        if (cmd->value) free(cmd->value);
        free(cmd);
    }
}

// Funzione di utilità per stampare il comando parsato
void print_parsed_command(const parsed_command_t *cmd) {
    if (!cmd) {
        printf("Comando NULL\n");
        return;
    }

    printf("Comando: %s\n", command_names[cmd->command]);

    if (cmd->key) {
        printf("Key: \"%s\" (len: %zu)\n", cmd->key, cmd->key_len);
    } else {
        printf("Key: (nessuna)\n");
    }

    if (cmd->value) {
        printf("Value: \"%s\" (len: %zu)\n", cmd->value, cmd->value_len);
    } else {
        printf("Value: (nessuno)\n");
    }
    printf("\n");
}

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 28/07/25
 * License: AGPL 3
 */

#include "resp_parser.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Inizializza buffer
static inline buffer_t buffer_init(const char *data, size_t len) {
    return (buffer_t){ .data = data, .len = len, .pos = 0 };
}

// Controlla se ci sono abbastanza byte rimanenti
static inline bool buffer_has_bytes(const buffer_t *buf, size_t count) {
    return buf->pos + count <= buf->len;
}

// Legge un byte senza avanzare
static inline char buffer_peek(const buffer_t *buf) {
    return buffer_has_bytes(buf, 1) ? buf->data[buf->pos] : '\0';
}

// Legge un byte e avanza
static inline char buffer_read_byte(buffer_t *buf) {
    return buffer_has_bytes(buf, 1) ? buf->data[buf->pos++] : '\0';
}

// Cerca CRLF dalla posizione corrente
static const char* buffer_find_crlf(const buffer_t *buf) {
    for (size_t i = buf->pos; i < buf->len - 1; i++) {
        if (buf->data[i] == '\r' && buf->data[i + 1] == '\n') {
            return &buf->data[i];
        }
    }
    return NULL;
}

// Avanza la posizione del buffer
static inline void buffer_skip(buffer_t *buf, size_t count) {
    buf->pos = (buf->pos + count > buf->len) ? buf->len : buf->pos + count;
}



// Legge un intero fino a CRLF
static parse_result_t read_integer(buffer_t *buf, int *result) {
    const char *crlf = buffer_find_crlf(buf);
    if (!crlf) return PARSE_INCOMPLETE;

    size_t num_len = crlf - (buf->data + buf->pos);
    if (num_len == 0 || num_len > 20) return PARSE_ERROR;

    // Parsing sicuro dell'intero
    char *endptr;
    errno = 0;
    long val = strtol(buf->data + buf->pos, &endptr, 10);

    // Verifica errori di conversione
    if (errno != 0 || endptr != crlf || val < INT_MIN || val > INT_MAX) {
        return PARSE_ERROR;
    }

    *result = (int)val;
    buf->pos = (crlf - buf->data) + 2; // Salta CRLF
    return PARSE_OK;
}

// Alloca e copia una stringa
static char* strndup_safe(const char *src, size_t len) {
    char *dest = malloc(len + 1);
    if (!dest) return NULL;

    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}

// Legge una bulk string
static parse_result_t read_bulk_string(buffer_t *buf, char **result) {
    if (buffer_peek(buf) != '$') return PARSE_ERROR;

    buffer_skip(buf, 1); // Salta '$'

    int str_len;
    parse_result_t res = read_integer(buf, &str_len);
    if (res != PARSE_OK) return res;

    // NULL bulk string
    if (str_len == -1) {
        *result = NULL;
        return PARSE_OK;
    }

    if (str_len < 0 || str_len > MAX_STR_LEN) return PARSE_ERROR;

    // Verifica che ci siano abbastanza byte per dati + CRLF
    if (!buffer_has_bytes(buf, str_len + 2)) return PARSE_INCOMPLETE;

    // Alloca e copia i dati
    *result = strndup_safe(buf->data + buf->pos, str_len);
    if (!*result) return PARSE_ERROR;

    // Verifica CRLF finale
    if (buf->data[buf->pos + str_len] != '\r' ||
        buf->data[buf->pos + str_len + 1] != '\n') {
        free(*result);
        *result = NULL;
        return PARSE_ERROR;
    }

    buffer_skip(buf, str_len + 2);
    return PARSE_OK;
}

// Libera un array di stringhe
static void free_string_array(char **strings, int count) {
    if (!strings) return;

    for (int i = 0; i < count; i++) {
        free(strings[i]);
    }
    free(strings);
}

// Parsing principale
int resp_parse(const char *data, size_t len, resp_command_t *out) {
    if (!data || !out || len < MIN_BUFFER_SIZE) {
        return len < MIN_BUFFER_SIZE ? PARSE_INCOMPLETE : PARSE_ERROR;
    }

    // Inizializza output
    *out = (resp_command_t){0};

    buffer_t buf = buffer_init(data, len);

    // Deve iniziare con '*'
    if (buffer_read_byte(&buf) != '*') return PARSE_ERROR;

    // Legge numero di elementi
    int num_elements;
    parse_result_t res = read_integer(&buf, &num_elements);
    if (res != PARSE_OK) return res;

    if (num_elements <= 0 || num_elements > MAX_ARGS) return PARSE_ERROR;

    // Alloca array per tutti gli elementi
    char **elements = calloc(num_elements, sizeof(char*));
    if (!elements) return PARSE_ERROR;

    // Legge tutti gli elementi
    for (int i = 0; i < num_elements; i++) {
        res = read_bulk_string(&buf, &elements[i]);
        if (res != PARSE_OK) {
            free_string_array(elements, i); // Libera solo quelli letti
            return res;
        }
    }

    // Assegna comando (primo elemento)
    out->command = elements[0];

    // Assegna argomenti (resto degli elementi)
    if (num_elements > 1) {
        out->arg_count = num_elements - 1;
        out->args = malloc(out->arg_count * sizeof(char*));
        if (!out->args) {
            free_string_array(elements, num_elements);
            return PARSE_ERROR;
        }

        memcpy(out->args, elements + 1, out->arg_count * sizeof(char*));
    }

    free(elements); // Libera solo l'array, non le stringhe
    return (int)buf.pos; // Byte consumati
}

void resp_command_free(resp_command_t *cmd) {
    if (!cmd) return;

    free(cmd->command);
    free_string_array(cmd->args, cmd->arg_count);

    *cmd = (resp_command_t){0}; // Reset completo
}

// Lookup table per comandi (più efficiente di strcasecmp multipli)
static const struct {
    const char *name;
    resp_command_e value;
} command_table[] = {
    {"PING", RESP_PING},
    {"QUIT", RESP_QUIT},
    {"SET", RESP_SET},
    {"GET", RESP_GET},
    {"DEL", RESP_DEL},
    {"CLIENT", RESP_CLIENT},
    { "INCR", RESP_INCR},
    {NULL, RESP_UNKNOW}
};

// Converte stringa a maiuscolo in-place (per confronto)
static void str_toupper(char *str) {
    for (; *str; str++) {
        *str = toupper((unsigned char)*str);
    }
}

resp_command_e resp_decode_command(const char *command) {
    if (!command) return RESP_UNKNOW;

    // Copia e converte a maiuscolo per confronto
    size_t len = strlen(command);
    if (len > 32) return RESP_UNKNOW; // Comando troppo lungo

    char upper_cmd[33];
    strcpy(upper_cmd, command);
    str_toupper(upper_cmd);

    // Cerca nella lookup table
    for (int i = 0; command_table[i].name; i++) {
        if (strcmp(upper_cmd, command_table[i].name) == 0) {
            return command_table[i].value;
        }
    }

    return RESP_UNKNOW;
}
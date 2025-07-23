/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 22/07/25
 * License: MIT
 */
#include <stdio.h>
#include <string.h>

#include "resp.h"

int parse_length(char *start, char *end) {
    int counter = 0;
    char len[10] = {'\0'};
    while (start < end) {
        len[counter++] = *start;
        start++;
    }
    return atoi(len);
}

char *parse_value(char *start, int len) {
    char *value = malloc(len + 1);  // +1 per null terminator
    if (!value) return NULL;

    memcpy(value, start, len);      // Più efficiente di loop
    value[len] = '\0';              // Null terminator!

    return value;
}

char *parse_bulk_string(resp_command_raw_t *command_raw, int *output_len) {

    char *p = command_raw->current_position;
    if (*p != '$') return NULL;
    p++;

    char *s = strchr(p, '\r');
    command_raw->current_position = s + 2;

    int len = parse_length(p, s);

    p = command_raw->current_position;
    s = strchr(p+1, '\r');
    command_raw->current_position = s + 2;
    char *value = parse_value(p, len);

    command_raw->current_position = s + 2;

    if (len != strlen(value)) return NULL;
    *output_len = len;
    return value;
}

int parse_array(resp_command_raw_t *command_raw, int *output_len) {

    char *p = command_raw->current_position;
    if (*p != '*') return -1;
    p++;

    char *s = strchr(p+1, '\r');
    command_raw->current_position = s + 2;

    *output_len = parse_length(p, s);
    return 0;
}

int main(void) {
    printf("test resp\n");

    //resp_command_raw_t command_raw = resp_command_create("*31\r\n$34\r\nSET\r\n$5\r\nmykey\r\n$5\r\nvalue\r\n");
    resp_command_raw_t command_raw = resp_command_create("$3\r\nSET\r\n");
    printf("pointer of command_raw.command = %p\n", command_raw.command);
    printf("pointer of command_raw.p = %p\n", command_raw.current_position);
    printf("lunghezza del comando : %lu\n", command_raw.command_length);

    int len = 0;
    command_raw.current_position++;
    char *output = parse_bulk_string(&command_raw, &len);
    printf("parsing di %s = %s\n", "$3\\r\\nSET\\r\\n", output);

    resp_command_raw_t command_a = resp_command_create("*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$5\r\nvalue\r\n");
    int pa = parse_array(&command_a, &len);
    printf("parsing di array di %d elementi\n", len);
    for (int i=0 ; i < len ; i++) {
        int l = 0;
        char *o = parse_bulk_string(&command_a, &l);
        printf("[%d] value = %s di lunghezza %d\n", i, o, l);
        free(o);

    }
}
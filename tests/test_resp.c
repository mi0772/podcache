/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 22/07/25
 * License: AGPL 3
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "resp_parser.h"

void test_command(char *command) {


    resp_command_t cmd;

    if (resp_parse(command, strlen(command), &cmd) > 0) {
        printf("Comando: %s\n", cmd.command);
        for (int i = 0; i < cmd.arg_count; i++) {
            printf("Arg[%d]: %s\n", i, cmd.args[i]);
        }
        // qui colleghi con la tua cache
        resp_command_free(&cmd);
    } else {
        fprintf(stderr, "Parsing fallito\n");
    }


}

int main(void) {
    test_command("*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n");
    test_command("*4\r\n$6\r\nCLIENT\r\n$7\r\nSETINFO\r\n$8\r\nLIB-NAME\r\n$5\r\njedis\r\n");
    return 0 ;
}
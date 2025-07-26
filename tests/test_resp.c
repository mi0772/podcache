/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 22/07/25
 * License: MIT
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "resp.h"

int main(void) {
    printf("test resp command\n");

    //simple string
    char *command = "+OK\r\n";
    printf("command is %s", command);
    resp_command_t resp_command = {
        .command = strdup(command)
    };
    resp_command.command_pos = resp_command.command;

    parse_resp_command(&resp_command);

    // test error
    char *command_error = "-Error message\r\n";
    printf("command is %s", command);
    resp_command.command = strdup(command_error);
    resp_command.command_pos = resp_command.command;

    parse_resp_command(&resp_command);


    // test integer
    char *command_int = ":+32\r\n";
    printf("command is %s", command);
    resp_command.command = strdup(command_int);
    resp_command.command_pos = resp_command.command;

    parse_resp_command(&resp_command);

    // test bulk string

    char *command_bs = "$5\r\nhello\r\n";
    printf("command is %s", command);
    resp_command.command = strdup(command_bs);
    resp_command.command_pos = resp_command.command;

    parse_resp_command(&resp_command);
}

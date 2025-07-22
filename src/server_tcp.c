#include "server_tcp.h"


#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "clogger.h"

/* Global state */
static volatile int server_running = 1;
static int server_socket = -1;

/* Command parsing */
command_type_t parse_command_type(const char *cmd) {
    if (strcasecmp(cmd, "PING") == 0) return CMD_PING;
    if (strcasecmp(cmd, "SET") == 0)  return CMD_SET;
    if (strcasecmp(cmd, "GET") == 0)  return CMD_GET;
    if (strcasecmp(cmd, "QUIT") == 0) return CMD_QUIT;
    if (strcasecmp(cmd, "INFO") == 0) return CMD_INFO;
    return CMD_UNKNOWN;
}

int parse_command_line(const char *line, parsed_command_t *cmd) {
    memset(cmd, 0, sizeof(parsed_command_t));
    
    char *line_copy = strdup(line);
    char *token = strtok(line_copy, " \t\r\n");
    
    if (!token) {
        free(line_copy);
        return 0;
    }
    
    cmd->type = parse_command_type(token);
    cmd->args[cmd->argc++] = strdup(token);
    
    while ((token = strtok(NULL, " \t\r\n")) && cmd->argc < 8) {
        cmd->args[cmd->argc++] = strdup(token);
    }
    
    free(line_copy);
    return 1;
}

void free_parsed_command(parsed_command_t *cmd) {
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->args[i]);
    }
}

/* Protocol response helpers */
int send_response(int socket, const char *format, ...) {
    char buffer[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    return send(socket, buffer, len, MSG_NOSIGNAL);
}

int send_ok_response(int socket, const char *message) {
    return send_response(socket, "+%s\r\n", message ? message : "OK");
}

int send_error_response(int socket, const char *error) {
    return send_response(socket, "-ERR %s\r\n", error);
}

int send_string_response(int socket, const char *str) {
    if (!str) {
        return send_response(socket, "$-1\r\n");  // NULL response
    }
    return send_response(socket, "$%zu\r\n%s\r\n", strlen(str), str);
}

/* Command handlers */
int handle_ping_command(client_ctx_t *client, parsed_command_t *cmd) {
    const char *message = (cmd->argc > 1) ? cmd->args[1] : "PONG";
    return send_ok_response(client->socket, message);
}

int handle_set_command(client_ctx_t *client, parsed_command_t *cmd) {
    if (cmd->argc < 3) {
        return send_error_response(client->socket, "wrong number of arguments for 'set' command");
    }
    
    store_set(cmd->args[1], cmd->args[2]);
    LOG_INFO("Client %s: SET %s = %s", client->client_id, cmd->args[1], cmd->args[2]);
    
    return send_ok_response(client->socket, NULL);
}

int handle_get_command(client_ctx_t *client, parsed_command_t *cmd) {
    if (cmd->argc < 2) {
        return send_error_response(client->socket, "wrong number of arguments for 'get' command");
    }
    
    char *value = store_get(cmd->args[1]);
    LOG_INFO("Client %s: GET %s = %s", client->client_id, cmd->args[1], value ? value : "(nil)");
    
    int result = send_string_response(client->socket, value);
    free(value);
    
    return result;
}

int handle_info_command(client_ctx_t *client, parsed_command_t *cmd) {
    pthread_mutex_lock(&store_mutex);
    int count = store_count;
    pthread_mutex_unlock(&store_mutex);
    
    char info[512];
    snprintf(info, sizeof(info), 
             "# Server Info\r\n"
             "version:1.0.0\r\n"
             "keys_count:%d\r\n"
             "connected_clients:active\r\n",
             count);
    
    return send_string_response(client->socket, info);
}

/* Main command dispatcher */
int handle_client_command(client_ctx_t *client, const char *line) {
    parsed_command_t cmd;
    
    if (!parse_command_line(line, &cmd)) {
        return send_error_response(client->socket, "empty command");
    }
    
    int result = 0;
    
    switch (cmd.type) {
        case CMD_PING:
            result = handle_ping_command(client, &cmd);
            break;
            
        case CMD_SET:
            result = handle_set_command(client, &cmd);
            break;
            
        case CMD_GET:
            result = handle_get_command(client, &cmd);
            break;
            
        case CMD_INFO:
            result = handle_info_command(client, &cmd);
            break;
            
        case CMD_QUIT:
            send_ok_response(client->socket, "BYE");
            free_parsed_command(&cmd);
            return -1;  // Signal to close connection
            
        case CMD_UNKNOWN:
        default:
            result = send_error_response(client->socket, "unknown command");
            break;
    }
    
    free_parsed_command(&cmd);
    return result;
}

/* Client thread handler */
void* handle_client_thread(void *arg) {
    client_ctx_t *client = (client_ctx_t*)arg;
    char buffer[BUFFER_SIZE];
    char line_buffer[MAX_LINE_LENGTH];
    int line_pos = 0;
    ssize_t bytes_received;
    
    log_info("Client %s connected", client->client_id);
    
    // Send welcome message
    send_ok_response(client->socket, "PodCache Server Ready");
    
    while (server_running && 
           (bytes_received = recv(client->socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        
        for (int i = 0; i < bytes_received; i++) {
            char c = buffer[i];
            
            if (c == '\n' || c == '\r') {
                if (line_pos > 0) {
                    line_buffer[line_pos] = '\0';
                    
                    // Process the command
                    if (handle_client_command(client, line_buffer) < 0) {
                        goto cleanup;  // Client requested disconnect
                    }
                    
                    line_pos = 0;
                }
            } else if (line_pos < MAX_LINE_LENGTH - 1) {
                line_buffer[line_pos++] = c;
            }
        }
    }
    
cleanup:
    log_info("Client %s disconnected", client->client_id);
    close(client->socket);
    free(client);
    return NULL;
}

/* Signal handlers */
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        log_info("Received signal %d, shutting down server...", sig);
        server_running = 0;
        
        if (server_socket >= 0) {
            close(server_socket);
        }
    }
}

/* Server configuration */
int get_server_port() {
    char *env_port = getenv("PODCACHE_SERVER_PORT");
    if (!env_port) return DEFAULT_PORT;
    
    int port = atoi(env_port);
    return (port > 0 && port <= 65535) ? port : DEFAULT_PORT;
}

/* Main server loop */
int tcp_server_start(void) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int server_port = get_server_port();
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe
    
    log_info("Starting PodCache Server v1.0.0");
    
    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        log_error("Failed to create socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_error("Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        log_error("Failed to bind to port %d: %s", server_port, strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_socket, MAX_PENDING_CONNS) == -1) {
        log_error("Failed to listen: %s", strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    log_info("Server listening on port %d", server_port);
    log_info("Ready to accept connections...");
    
    // Main accept loop
    while (server_running) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_socket == -1) {
            if (server_running) {
                log_error("Failed to accept connection: %s", strerror(errno));
            }
            continue;
        }
        
        // Create client context
        client_ctx_t *client = malloc(sizeof(client_ctx_t));
        if (!client) {
            log_error("Failed to allocate memory for client");
            close(client_socket);
            continue;
        }
        
        client->socket = client_socket;
        client->addr = client_addr;
        snprintf(client->client_id, sizeof(client->client_id), "%s:%d", 
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Create thread for client
        if (pthread_create(&client->thread_id, NULL, handle_client_thread, client) != 0) {
            log_error("Failed to create thread for client %s", client->client_id);
            close(client_socket);
            free(client);
            continue;
        }
        
        // Detach thread so it cleans up automatically
        pthread_detach(client->thread_id);
    }
    
    // Cleanup
    log_info("Cleaning up resources...");
    // distruzione della cache

    close(server_socket);
    log_info("Server shutdown complete");
    
    return 0;
}
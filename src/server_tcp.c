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
#include "pod_cache.h"
#include "server_command.h"

/* Global state */
static volatile int server_running = 1;
static int server_socket = -1;

static pod_cache_t *cache;

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
    return send_response(socket, "%s\r\n", message ? message : "OK");
}

int send_error_response(int socket, const char *error) {
    return send_response(socket, "KO %s\r\n", error);
}

int send_string_response(int socket, const char *str) {
    if (!str) {
        return send_response(socket, "$-1\r\n");  // NULL response
    }
    return send_response(socket, "$%zu\r\n%s\r\n", strlen(str), str);
}

/* Command handlers */
int handle_ping_command(client_ctx_t *client, parsed_command_t *cmd) {
    const char *message = "PONG\r\n";
    return send_ok_response(client->socket, message);
}

int handle_set_command(client_ctx_t *client, parsed_command_t *cmd) {
    if (cmd->key_len <= 0 || cmd->value_len <= 0) {
        return send_error_response(client->socket, "wrong number of arguments for 'set' command");
    }
    

    log_debug("Client %s: SET %s = %s", client->client_id, cmd->key, cmd->value);
    pod_cache_put(cache, cmd->key, cmd->value, cmd->value_len);
    
    return send_ok_response(client->socket, NULL);
}

int handle_get_command(client_ctx_t *client, parsed_command_t *cmd) {
    if (cmd->key_len <= 0) {
        return send_error_response(client->socket, "wrong number of arguments for 'get' command");
    }
    

    log_debug("Client %s: GET %s", client->client_id, cmd->key);
    void *value = NULL;
    size_t value_size = 0;
    pod_cache_get(cache, cmd->key, &value, &value_size );
    int result = send_string_response(client->socket, (char *) value);
    //free(value);
    
    return result;
}

int handle_info_command(client_ctx_t *client, parsed_command_t *cmd) {

    int count = 100; //store_count;

    
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
    parsed_command_t *cmd = parse_command(line);

    if (!cmd) {
        return send_error_response(client->socket, "unknown command");
    }
    int result = 0;

    switch (cmd->command) {
        case CMD_PING:
            result = handle_ping_command(client, cmd);
            break;
            
        case CMD_PUT:
            result = handle_set_command(client, cmd);
            break;
            
        case CMD_GET:
            result = handle_get_command(client, cmd);
            break;
            
        case CMD_INFO:
            result = handle_info_command(client, cmd);
            break;
            
        case CMD_QUIT:
            send_ok_response(client->socket, "BYE");
            free_parsed_command(cmd);
            return -1;  // Signal to close connection
            
        case CMD_UNKNOWN:
        default:
            result = send_error_response(client->socket, "unknown command");
            break;
    }
    
    free_parsed_command(cmd);
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

    log_info("creating base cache with size %lu and %d partitions", MB_TO_BYTES(100), 3);
    if (cache != NULL) {
        log_warn("cache already exist, destroy it before creating a new onw");
        pod_cache_destroy(cache);
    }
    cache = pod_cache_create(MB_TO_BYTES(100), 3);
    if (cache)
        log_info("base cache created");
    else {
        log_error("cannot create cache");
        exit(9);
    }
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
    pod_cache_destroy(cache);

    close(server_socket);
    log_info("Server shutdown complete");
    
    return 0;
}
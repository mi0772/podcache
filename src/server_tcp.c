/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 28/07/25
 * License: MIT
 */

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
#include "resp_parser.h"

static server_state_t g_server = {0};

// Forward declarations
static void cleanup_server(void);
static int setup_server_socket(int port);
static void* client_handler_thread(void* arg);
static int send_formatted_response(int socket_fd, const char *format, ...);
static int send_integer_response(int socket_fd, long val);
static int send_ok_response(int socket_fd, const char *message);
static int send_error_response(int socket_fd, const char *error);
static int send_bulk_string_response(int socket_fd, const char *str);
static int handle_ping(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int handle_set(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int handle_get(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int handle_quit(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int handle_client_cmd(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int handle_incr(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int handle_del(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static int dispatch_command(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
static void buffer_init(command_buffer_t *buf);
static bool buffer_append(command_buffer_t *buf, const void *data, size_t len);
static void buffer_consume(command_buffer_t *buf, size_t bytes);
static client_ctx_t* create_client_context(int socket_fd, struct sockaddr_in *addr);
static void destroy_client_context(client_ctx_t *client);
static void* client_handler_thread(void *arg);
static int get_env_int(const char *env_name, int default_value, int min_val, int max_val);
static int get_server_port(void);
static pod_cache_t* initialize_cache(void);
static void signal_handler(int sig);
static int setup_server_socket(int port);
static void cleanup_server(void);
static void setup_signal_handlers(void);
static void* display_cache_status(void* args);

// Command dispatch table
static const command_handler_t command_handlers[] = {
    {RESP_PING, "PING", handle_ping},
    {RESP_SET, "SET", handle_set},
    {RESP_GET, "GET", handle_get},
    {RESP_QUIT, "QUIT", handle_quit},
    {RESP_CLIENT, "CLIENT", handle_client_cmd},
    {RESP_INCR, "INCR", handle_incr},
    { RESP_DEL, "DEL", handle_del},
    {RESP_UNLINK, "UNLINK", handle_del},
    {RESP_UNKNOW, NULL, NULL} // Sentinel
};

/* public function implementation */

int tcp_server_start(void) {
    // Initialize server state
    g_server.running = 1;
    g_server.socket_fd = -1;
    g_server.cache = NULL;

    // Setup cleanup handler
    atexit(cleanup_server);
    setup_signal_handlers();

    log_info("Starting PodCache Server v1.0.0");

    // Initialize cache
    g_server.cache = initialize_cache();

    if (!g_server.cache) {
        return EXIT_FAILURE;
    }

    pthread_t thread_cache_status;
    if (pthread_create(&thread_cache_status, NULL, display_cache_status, g_server.cache) != 0) {
        log_error("failed to create thread for cache stats");
    }
    pthread_detach(thread_cache_status);

    // Setup server socket
    int port = get_server_port();
    g_server.socket_fd = setup_server_socket(port);
    if (g_server.socket_fd == -1) {
        return EXIT_FAILURE;
    }

    log_info("Server listening on port %d", port);
    log_info("Ready to accept connections...");

    // Main accept loop
    while (g_server.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(g_server.socket_fd,
                              (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd == -1) {
            if (g_server.running && errno != EINTR) {
                log_error("Failed to accept connection: %s", strerror(errno));
            }
            continue;
        }

        // Create client context
        client_ctx_t *client = create_client_context(client_fd, &client_addr);
        if (!client) {
            log_error("Failed to create client context");
            close(client_fd);
            continue;
        }

        // Create thread parameters
        server_thread_params_t *params = malloc(sizeof(server_thread_params_t));
        if (!params) {
            log_error("Failed to allocate thread parameters");
            destroy_client_context(client);
            continue;
        }

        params->client_ctx = client;
        params->cache = g_server.cache;

        // Create client thread
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler_thread, params) != 0) {
            log_error("Failed to create thread for client %s", client->client_id);
            destroy_client_context(client);
            free(params);
            continue;
        }



        // Detach thread for automatic cleanup
        pthread_detach(thread_id);
    }

    return EXIT_SUCCESS;
}


// static function implementation

// === PROTOCOL HELPERS ===

static void* display_cache_status(void* args) {
    pod_cache_t *cache = (pod_cache_t *)args;
    while (1) {
        sleep(10);
        time_t now;
        time(&now);
        log_info("cache status");
        for (int i=0 ; i < cache->partition_count ; i++) {
            log_info("status -> partition number %d, mb used : %.6f on %.3f", i, BYTES_TO_MB(cache->partitions[i]->current_bytes_size), BYTES_TO_MB(cache->partitions[i]->max_bytes_capacity));
        }
    }
}

static int send_formatted_response(int socket_fd, const char *format, ...) {
    char buffer[BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len >= BUFFER_SIZE) {
        log_error("Response too large to send");
        return -1;
    }

    return send(socket_fd, buffer, len, MSG_NOSIGNAL);
}

static int send_integer_response(int socket_fd, long val) {
    return send_formatted_response(socket_fd, ":%lu\r\n", val);
}

static int send_ok_response(int socket_fd, const char *message) {
    return send_formatted_response(socket_fd, "+%s\r\n", message ?: "OK");
}

static int send_error_response(int socket_fd, const char *error) {
    return send_formatted_response(socket_fd, "-ERR %s\r\n", error);
}

static int send_bulk_string_response(int socket_fd, const char *str) {
    if (!str) return send_formatted_response(socket_fd, "$-1\r\n");

    size_t len = strlen(str);
    return send_formatted_response(socket_fd, "$%zu\r\n%s\r\n", len, str);
}

// === COMMAND HANDLERS ===

static int handle_ping(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    (void)cache; (void)cmd; // Unused parameters
    return send_ok_response(client->socket, "PONG");
}

static int handle_incr(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 1) return send_error_response(client->socket,"wrong number of arguments for 'INCR' command");

    const char *key = cmd->args[0];
    void *value = NULL;
    size_t value_size = 0;

    int result = pod_cache_get(cache, key, &value, &value_size);
    if (result != 0) {
        //chiave non presente, la memorizzo nuova con valore 1
        pod_cache_put(cache, key, "1", 1);
        return send_integer_response(client->socket, 1);
    }

    char *endptr;
    errno = 0;
    long val = strtol(value, &endptr, 10);

    if (errno != 0 || *endptr != '\0') return send_error_response(client->socket, "value is not an integer or out of range");

    val++;
    char buffer[24];
    sprintf(buffer, "%ld", val);

    pod_cache_put(cache, key, buffer, strlen(buffer));
    return send_integer_response(client->socket, val);
}

static int handle_del(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 1) return send_error_response(client->socket,"wrong number of arguments for 'DEL' or 'UNLINK' command");

    int evict_result = pod_cache_evict(cache, cmd->args[0]);
    if (evict_result != -1) return send_integer_response(client->socket, evict_result);

    return send_error_response(client->socket, "error");

}

static int handle_set(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 2) {
        return send_error_response(client->socket,
            "wrong number of arguments for 'SET' command");
    }

    const char *key = cmd->args[0];
    const char *value = cmd->args[1];

    log_debug("Client %s: SET %s = %s", client->client_id, key, value);

    if (pod_cache_put(cache, key, value, strlen(value)) != 0) {
        return send_error_response(client->socket, "failed to store value");
    }

    return send_ok_response(client->socket, NULL);
}

static int handle_get(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 1) {
        return send_error_response(client->socket,
            "wrong number of arguments for 'GET' command");
    }

    const char *key = cmd->args[0];
    log_debug("Client %s: GET %s", client->client_id, key);

    void *value = NULL;
    size_t value_size = 0;

    int result = pod_cache_get(cache, key, &value, &value_size);
    if (result != 0) {
        return send_bulk_string_response(client->socket, NULL); // Not found
    }

    int send_result = send_bulk_string_response(client->socket, (char*)value);
    free(value);

    return send_result;
}

static int handle_quit(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    (void)cache; (void)cmd; // Unused parameters
    send_ok_response(client->socket, "BYE");
    return -1; // Signal client disconnect
}

/* handler per gestire il coment CLIENT (jedis), non viene gestito e quindi restituiamo sempre +OK */
static int handle_client_cmd(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    (void)cache; // Unused parameter

    // rispondiamo sempre +OK
    return send_ok_response(client->socket, NULL);
}

static int dispatch_command(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    resp_command_e cmd_type = resp_decode_command(cmd->command);

    for (const command_handler_t *handler = command_handlers; handler->name; handler++) {
        if (handler->type == cmd_type) {
            return handler->handler(client, cache, cmd);
        }
    }

    return send_error_response(client->socket, "unknown command");
}

// === CLIENT HANDLING ===


static void buffer_init(command_buffer_t *buf) {
    buf->used = 0;
    buf->capacity = sizeof(buf->buffer);
}

static bool buffer_append(command_buffer_t *buf, const void *data, size_t len) {
    if (buf->used + len > buf->capacity) {
        return false; // Overflow
    }

    memcpy(buf->buffer + buf->used, data, len);
    buf->used += len;
    return true;
}

static void buffer_consume(command_buffer_t *buf, size_t bytes) {
    if (bytes >= buf->used) {
        buf->used = 0;
        return;
    }

    memmove(buf->buffer, buf->buffer + bytes, buf->used - bytes);
    buf->used -= bytes;
}

static client_ctx_t* create_client_context(int socket_fd, struct sockaddr_in *addr) {
    client_ctx_t *client = calloc(1, sizeof(client_ctx_t));
    if (!client) return NULL;

    client->socket = socket_fd;
    client->addr = *addr;

    snprintf(client->client_id, sizeof(client->client_id), "%s:%d",
             inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));

    return client;
}

static void destroy_client_context(client_ctx_t *client) {
    if (!client) return;

    if (client->socket >= 0) {
        close(client->socket);
    }
    free(client);
}

static void* client_handler_thread(void *arg) {
    server_thread_params_t *params = (server_thread_params_t*)arg;
    client_ctx_t *client = params->client_ctx;
    pod_cache_t *cache = params->cache;

    command_buffer_t cmd_buf;
    buffer_init(&cmd_buf);

    char recv_buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    log_info("Client %s connected", client->client_id);

    while (g_server.running &&
           (bytes_received = recv(client->socket, recv_buffer,
                                sizeof(recv_buffer), 0)) > 0) {

        log_debug("Received %zd bytes from %s", bytes_received, client->client_id);

        // Check for buffer overflow
        if (!buffer_append(&cmd_buf, recv_buffer, bytes_received)) {
            log_error("Command buffer overflow for client %s", client->client_id);
            send_error_response(client->socket, "command too long");
            buffer_init(&cmd_buf); // Reset buffer
            continue;
        }

        // Process all complete commands in buffer
        size_t processed = 0;
        while (processed < cmd_buf.used) {
            resp_command_t cmd;

            int consumed = resp_parse(cmd_buf.buffer + processed,
                                    cmd_buf.used - processed, &cmd);

            if (consumed > 0) {
                // Command parsed successfully
                log_debug("Parsed command: %s (consumed %d bytes)",
                         cmd.command, consumed);

                int result = dispatch_command(client, cache, &cmd);
                resp_command_free(&cmd);

                if (result < 0) {
                    goto cleanup; // Client wants to disconnect
                }

                processed += consumed;

            } else if (consumed == 0) {
                // Incomplete command - wait for more data
                log_debug("Incomplete command, waiting for more data");
                break;

            } else {
                // Parse error
                log_error("Parse error for client %s", client->client_id);
                send_error_response(client->socket, "protocol error");
                processed = cmd_buf.used; // Discard all data
                break;
            }
        }

        // Remove processed data from buffer
        buffer_consume(&cmd_buf, processed);
    }

    if (bytes_received < 0 && errno != ECONNRESET) {
        log_error("Receive error from client %s: %s",
                 client->client_id, strerror(errno));
    }

cleanup:
    log_info("Client %s disconnected", client->client_id);
    destroy_client_context(client);
    return NULL;
}

// === SERVER CONFIGURATION ===

static int get_env_int(const char *env_name, int default_value, int min_val, int max_val) {
    const char *env_str = getenv(env_name);
    if (!env_str) return default_value;

    char *endptr;
    errno = 0;
    long val = strtol(env_str, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || val < min_val || val > max_val) {
        log_warn("Invalid value for %s: %s, using default %d",
                   env_name, env_str, default_value);
        return default_value;
    }

    return (int)val;
}

static int get_server_port(void) {
    return get_env_int("PODCACHE_SERVER_PORT", DEFAULT_PORT, 1024, 65535);
}

static pod_cache_t* initialize_cache(void) {
    int cache_size_mb = get_env_int("PODCACHE_SIZE", 100, 1, 4096);
    int partitions = get_env_int("PODCACHE_PARTITIONS", 1, 1, 64);

    log_info("Creating cache: %d MB, %d partitions", cache_size_mb, partitions);

    pod_cache_t *cache = pod_cache_create(MB_TO_BYTES(cache_size_mb), partitions);
    if (!cache) {
        log_error("Failed to create cache");
    }

    return cache;
}

// === SIGNAL HANDLING ===

static void signal_handler(int sig) {
    const char *sig_name = (sig == SIGINT) ? "SIGINT" :
                          (sig == SIGTERM) ? "SIGTERM" : "unknown";

    log_info("Received %s, shutting down server...", sig_name);
    g_server.running = 0;

    // Wake up accept() call
    if (g_server.socket_fd >= 0) {
        shutdown(g_server.socket_fd, SHUT_RDWR);
    }
}

static void setup_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        log_error("Failed to set signal handlers: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Ignore SIGPIPE to handle broken connections gracefully
    signal(SIGPIPE, SIG_IGN);
}

// === SERVER SETUP ===

static int setup_server_socket(int port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        log_error("Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_error("Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(sock_fd);
        return -1;
    }

    // Configure and bind address
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        log_error("Failed to bind to port %d: %s", port, strerror(errno));
        close(sock_fd);
        return -1;
    }

    if (listen(sock_fd, MAX_PENDING_CONNS) == -1) {
        log_error("Failed to listen: %s", strerror(errno));
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

static void cleanup_server(void) {
    log_info("Cleaning up server resources...");

    if (g_server.socket_fd >= 0) {
        close(g_server.socket_fd);
        g_server.socket_fd = -1;
    }

    if (g_server.cache) {
        pod_cache_destroy(g_server.cache);
        g_server.cache = NULL;
    }

    log_info("Server cleanup complete");
}

// === MAIN SERVER FUNCTION ===


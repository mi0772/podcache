/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 28/07/25
 * License: AGPL 3
 */

#include "server_tcp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
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
static void *client_handler_thread(void *arg);
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
static client_ctx_t *create_client_context(int socket_fd, struct sockaddr_in *addr);
static void destroy_client_context(client_ctx_t *client);
static void *client_handler_thread(void *arg);
static int get_env_int(const char *env_name, int default_value, int min_val, int max_val);
static int get_server_port(void);
static pod_cache_t *initialize_cache(void);
static void signal_handler(int sig);
static int setup_server_socket(int port);
static void cleanup_server(void);
static void setup_signal_handlers(void);
static void *display_cache_status(void *args);

// Command dispatch table
static const command_handler_t command_handlers[] = {
    {RESP_PING, "PING", handle_ping},
    {RESP_SET, "SET", handle_set},
    {RESP_GET, "GET", handle_get},
    {RESP_QUIT, "QUIT", handle_quit},
    {RESP_CLIENT, "CLIENT", handle_client_cmd},
    {RESP_INCR, "INCR", handle_incr},
    {RESP_DEL, "DEL", handle_del},
    {RESP_UNLINK, "UNLINK", handle_del},
    {RESP_UNKNOW, NULL, NULL} // Sentinel
};

/* public function implementation */

int tcp_server_start(void) {
    log_info("Starting PodCache TCP Server...");

    // Initialize server state
    g_server.running = 1;
    g_server.socket_fd = -1;
    g_server.cache = NULL;
    log_debug("Server state initialized");

    // Setup cleanup handler
    atexit(cleanup_server);
    setup_signal_handlers();
    log_debug("Signal handlers and cleanup registered");

    log_info("PodCache Server v1.0.0 - Initializing...");

    // Initialize cache
    g_server.cache = initialize_cache();

    if (!g_server.cache) {
        log_error("Failed to initialize cache, server startup aborted");
        return EXIT_FAILURE;
    }
    //log_info("Cache initialized successfully");

    pthread_t thread_cache_status;
    if (pthread_create(&thread_cache_status, NULL, display_cache_status, g_server.cache) != 0) {
        log_error("Failed to create cache status monitoring thread");
    } else {
        log_debug("Cache status monitoring thread created");
    }
    pthread_detach(thread_cache_status);

    // Setup server socket
    int port = get_server_port();
    log_debug("Server will bind to port: %d", port);
    g_server.socket_fd = setup_server_socket(port);
    if (g_server.socket_fd == -1) {
        log_error("Failed to setup server socket on port %d", port);
        return EXIT_FAILURE;
    }

    log_info("Server successfully bound and listening on port %d", port);
    log_info("Server ready to accept client connections");

    // Main accept loop
    while (g_server.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        log_debug("Waiting for client connection...");
        int client_fd = accept(g_server.socket_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd == -1) {
            if (!g_server.running) {
                // Server is shutting down, this is expected
                log_debug("Accept interrupted due to server shutdown");
                break;
            }
            if (errno == EINTR) {
                // Interrupted by signal, check if server should continue
                log_debug("Accept interrupted by signal");
                continue;
            }
            if (errno == EBADF || errno == ENOTSOCK) {
                // Socket was closed, probably due to shutdown
                log_debug("Accept failed: socket closed during shutdown");
                break;
            }
            log_error("Failed to accept client connection: %s", strerror(errno));
            continue;
        }

        // Log client connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        log_info("New client connected from %s:%d (fd: %d)", client_ip, ntohs(client_addr.sin_port),
                 client_fd);

        // Create client context
        client_ctx_t *client = create_client_context(client_fd, &client_addr);
        if (!client) {
            log_error("Failed to create client context for client %s:%d", client_ip,
                      ntohs(client_addr.sin_port));
            close(client_fd);
            continue;
        }
        log_debug("Created client context for client ID: %s", client->client_id);

        // Create thread parameters
        server_thread_params_t *params = malloc(sizeof(server_thread_params_t));
        if (!params) {
            log_error("Failed to allocate thread parameters for client %s", client->client_id);
            destroy_client_context(client);
            continue;
        }

        params->client_ctx = client;
        params->cache = g_server.cache;

        // Create client thread
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler_thread, params) != 0) {
            log_error("Failed to create handler thread for client %s", client->client_id);
            destroy_client_context(client);
            free(params);
            continue;
        }

        log_debug("Created handler thread for client %s", client->client_id);

        // Detach thread for automatic cleanup
        pthread_detach(thread_id);
    }

    log_info("Server shutting down...");
    return EXIT_SUCCESS;
}

// static function implementation

// === PROTOCOL HELPERS ===

static void *display_cache_status(void *args) {
    pod_cache_t *cache = (pod_cache_t *)args;
    log_debug("Cache status monitoring thread started");

    while (1) {
        sleep(10);
        time_t now;
        time(&now);
        log_info("=== Cache Status Report ===");
        for (int i = 0; i < cache->partition_count; i++) {
            double used_mb = BYTES_TO_MB(cache->partitions[i]->current_bytes_size);
            double total_mb = BYTES_TO_MB(cache->partitions[i]->max_bytes_capacity);
            double usage_percent = (used_mb / total_mb) * 100.0;
            log_info("Partition %d: %.2f MB used / %.2f MB total (%.1f%%)", i, used_mb, total_mb,
                     usage_percent);
        }
        log_info("=== End Cache Status ===");
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
    (void)cache;
    (void)cmd; // Unused parameters
    log_debug("Client %s: PING command received", client->client_id);
    return send_ok_response(client->socket, "PONG");
}

static int handle_incr(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 1) {
        log_warn("Client %s: INCR command with invalid arguments (count: %d)", client->client_id,
                 cmd->arg_count);
        return send_error_response(client->socket, "wrong number of arguments for 'INCR' command");
    }

    const char *key = cmd->args[0];
    log_debug("Client %s: INCR request for key '%s'", client->client_id, key);

    void *value = NULL;
    size_t value_size = 0;

    int result = pod_cache_get(cache, key, &value, &value_size);
    if (result != 0) {
        // chiave non presente, la memorizzo nuova con valore 1
        log_debug("Client %s: INCR key '%s' - not found, initializing to 1", client->client_id,
                  key);
        pod_cache_put(cache, key, "1", 1);
        return send_integer_response(client->socket, 1);
    }

    char *endptr;
    errno = 0;
    long val = strtol(value, &endptr, 10);

    if (errno != 0 || *endptr != '\0') {
        log_warn("Client %s: INCR key '%s' - value is not a valid integer", client->client_id, key);
        free(value);
        return send_error_response(client->socket, "value is not an integer or out of range");
    }

    val++;
    char buffer[24];
    sprintf(buffer, "%ld", val);

    log_debug("Client %s: INCR key '%s' - incremented to %ld", client->client_id, key, val);
    pod_cache_put(cache, key, buffer, strlen(buffer));
    free(value);
    return send_integer_response(client->socket, val);
}

static int handle_del(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 1) {
        log_warn("Client %s: DEL command with invalid arguments (count: %d)", client->client_id,
                 cmd->arg_count);
        return send_error_response(client->socket,
                                   "wrong number of arguments for 'DEL' or 'UNLINK' command");
    }

    const char *key = cmd->args[0];
    log_debug("Client %s: DEL request for key '%s'", client->client_id, key);

    int evict_result = pod_cache_evict(cache, key);
    if (evict_result != -1) {
        log_info("Client %s: DEL key '%s' - %s", client->client_id, key,
                 evict_result == 1 ? "deleted" : "not found");
        return send_integer_response(client->socket, evict_result);
    }

    log_error("Client %s: DEL key '%s' - error occurred", client->client_id, key);
    return send_error_response(client->socket, "error");
}

static int handle_set(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 2) {
        log_warn("Client %s: SET command with invalid arguments (count: %d)", client->client_id,
                 cmd->arg_count);
        return send_error_response(client->socket, "wrong number of arguments for 'SET' command");
    }

    const char *key = cmd->args[0];
    const char *value = cmd->args[1];
    size_t value_len = strlen(value);

    log_debug("Client %s: SET request - key='%s', value_size=%zu", client->client_id, key,
              value_len);

    int result = pod_cache_put(cache, key, (void *)value, value_len);
    if (result < 0) {
        log_warn("Client %s: SET failed for key '%s' - error code: %d", client->client_id, key,
                 result);
        return send_error_response(client->socket, "failed to store value");
    }

    log_info("Client %s: SET successful - key='%s', stored in partition=%d", client->client_id, key,
             result);
    return send_ok_response(client->socket, NULL);
}

static int handle_get(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    if (cmd->arg_count < 1) {
        log_warn("Client %s: GET command with invalid arguments (count: %d)", client->client_id,
                 cmd->arg_count);
        return send_error_response(client->socket, "wrong number of arguments for 'GET' command");
    }

    const char *key = cmd->args[0];
    log_debug("Client %s: GET request for key '%s'", client->client_id, key);

    void *value = NULL;
    size_t value_size = 0;

    int result = pod_cache_get(cache, key, &value, &value_size);
    if (result != 0) {
        log_debug("Client %s: GET key '%s' - not found", client->client_id, key);
        return send_bulk_string_response(client->socket, NULL); // Not found
    }

    log_debug("Client %s: GET key '%s' - found, size: %zu bytes", client->client_id, key,
              value_size);
    int send_result = send_bulk_string_response(client->socket, (char *)value);
    free(value);

    return send_result;
}

static int handle_quit(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    (void)cache;
    (void)cmd; // Unused parameters
    log_info("Client %s: QUIT command received, disconnecting", client->client_id);
    send_ok_response(client->socket, "BYE");
    return -1; // Signal client disconnect
}

/* handler per gestire il coment CLIENT (jedis), non viene gestito e quindi restituimo sempre +OK */
static int handle_client_cmd(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    (void)cache; // Unused parameter

    log_debug("Client %s: CLIENT command received", client->client_id);
    // rispondiamo sempre +OK
    return send_ok_response(client->socket, NULL);
}

static int dispatch_command(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd) {
    resp_command_e cmd_type = resp_decode_command(cmd->command);

    log_debug("Client %s: Dispatching command '%s'", client->client_id, cmd->command);

    for (const command_handler_t *handler = command_handlers; handler->name; handler++) {
        if (handler->type == cmd_type) {
            log_debug("Client %s: Found handler for command '%s'", client->client_id, cmd->command);
            return handler->handler(client, cache, cmd);
        }
    }

    log_warn("Client %s: Unknown command '%s'", client->client_id, cmd->command);
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

static client_ctx_t *create_client_context(int socket_fd, struct sockaddr_in *addr) {
    client_ctx_t *client = calloc(1, sizeof(client_ctx_t));
    if (!client) return NULL;

    client->socket = socket_fd;
    client->addr = *addr;

    snprintf(client->client_id, sizeof(client->client_id), "%s:%d", inet_ntoa(addr->sin_addr),
             ntohs(addr->sin_port));

    return client;
}

static void destroy_client_context(client_ctx_t *client) {
    if (!client) return;

    if (client->socket >= 0) {
        close(client->socket);
    }
    free(client);
}

static void *client_handler_thread(void *arg) {
    server_thread_params_t *params = (server_thread_params_t *)arg;
    client_ctx_t *client = params->client_ctx;
    pod_cache_t *cache = params->cache;

    command_buffer_t cmd_buf;
    buffer_init(&cmd_buf);

    char recv_buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    log_info("Client %s: Connection established, handler thread started", client->client_id);

    while (g_server.running &&
           (bytes_received = recv(client->socket, recv_buffer, sizeof(recv_buffer), 0)) > 0) {

        log_debug("Client %s: Received %zd bytes", client->client_id, bytes_received);

        // Check for buffer overflow
        if (!buffer_append(&cmd_buf, recv_buffer, bytes_received)) {
            log_error("Client %s: Command buffer overflow, resetting buffer", client->client_id);
            send_error_response(client->socket, "command too long");
            buffer_init(&cmd_buf); // Reset buffer
            continue;
        }

        // Process all complete commands in buffer
        size_t processed = 0;
        while (processed < cmd_buf.used) {
            resp_command_t cmd;

            int consumed = resp_parse(cmd_buf.buffer + processed, cmd_buf.used - processed, &cmd);

            if (consumed > 0) {
                // Command parsed successfully
                log_debug("Client %s: Parsed command '%s' (consumed %d bytes)", client->client_id,
                          cmd.command, consumed);

                int result = dispatch_command(client, cache, &cmd);
                resp_command_free(&cmd);

                if (result < 0) {
                    log_debug("Client %s: Command returned disconnect signal", client->client_id);
                    goto cleanup; // Client wants to disconnect
                }

                processed += consumed;

            } else if (consumed == 0) {
                // Incomplete command - wait for more data
                log_debug("Client %s: Incomplete command, waiting for more data",
                          client->client_id);
                break;

            } else {
                // Parse error
                log_error("Client %s: Protocol parse error, discarding buffer", client->client_id);
                send_error_response(client->socket, "protocol error");
                processed = cmd_buf.used; // Discard all data
                break;
            }
        }

        // Remove processed data from buffer
        buffer_consume(&cmd_buf, processed);
    }

    if (bytes_received < 0 && errno != ECONNRESET) {
        log_error("Client %s: Receive error - %s", client->client_id, strerror(errno));
    } else if (bytes_received == 0) {
        log_debug("Client %s: Connection closed by client", client->client_id);
    }

cleanup:
    log_info("Client %s: Disconnected, cleaning up resources", client->client_id);
    destroy_client_context(client);
    free(params);
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
        log_warn("Invalid value for %s: %s, using default %d", env_name, env_str, default_value);
        return default_value;
    }

    return (int)val;
}

static int get_server_port(void) {
    return get_env_int("PODCACHE_SERVER_PORT", DEFAULT_PORT, 1024, 65535);
}

static pod_cache_t *initialize_cache(void) {
    int cache_size_mb = get_env_int("PODCACHE_SIZE", 100, 1, 4096);
    int partitions = get_env_int("PODCACHE_PARTITIONS", 1, 1, 64);

    log_info("Initializing cache with %d MB capacity and %d partitions", cache_size_mb, partitions);
    log_debug("Cache configuration: total size = %zu bytes", MB_TO_BYTES(cache_size_mb));

    pod_cache_t *cache = pod_cache_create(MB_TO_BYTES(cache_size_mb), partitions);
    if (!cache) {
        log_error("Failed to create pod cache with %d MB and %d partitions", cache_size_mb,
                  partitions);
        return NULL;
    }

    log_info("Cache initialized successfully");
    return cache;
}

// === SIGNAL HANDLING ===

static void signal_handler(int sig) {
    const char *sig_name = (sig == SIGINT) ? "SIGINT" : (sig == SIGTERM) ? "SIGTERM" : "unknown";

    log_info("Received %s, shutting down server...", sig_name);
    g_server.running = 0;

    // Wake up accept() call by closing the socket
    if (g_server.socket_fd >= 0) {
        log_debug("Closing server socket to interrupt accept() call");
        shutdown(g_server.socket_fd, SHUT_RDWR);
        close(g_server.socket_fd);
        g_server.socket_fd = -1;
    }
}

static void setup_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    // Don't use SA_RESTART for SIGINT/SIGTERM - we want accept() to be interrupted
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        log_error("Failed to set signal handlers: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Ignore SIGPIPE to handle broken connections gracefully
    signal(SIGPIPE, SIG_IGN);

    log_debug("Signal handlers configured (SIGINT, SIGTERM, SIGPIPE ignored)");
}

// === SERVER SETUP ===

static int setup_server_socket(int port) {
    log_debug("Setting up server socket on port %d", port);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        log_error("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    log_debug("Socket created successfully (fd: %d)", sock_fd);

    // Set socket options
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_error("Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(sock_fd);
        return -1;
    }
    log_debug("Socket options configured (SO_REUSEADDR enabled)");

    // Configure and bind address
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        log_error("Failed to bind to port %d: %s", port, strerror(errno));
        close(sock_fd);
        return -1;
    }
    log_debug("Socket bound to port %d successfully", port);

    if (listen(sock_fd, MAX_PENDING_CONNS) == -1) {
        log_error("Failed to listen with backlog %d: %s", MAX_PENDING_CONNS, strerror(errno));
        close(sock_fd);
        return -1;
    }
    log_debug("Socket is listening with backlog %d", MAX_PENDING_CONNS);

    log_info("Server socket setup complete on port %d (fd: %d)", port, sock_fd);
    return sock_fd;
}

static void cleanup_server(void) {
    log_info("Server cleanup initiated...");

    if (g_server.socket_fd >= 0) {
        log_debug("Closing server socket (fd: %d)", g_server.socket_fd);
        close(g_server.socket_fd);
        g_server.socket_fd = -1;
    }

    if (g_server.cache) {
        log_debug("Destroying cache instance");
        pod_cache_destroy(g_server.cache);
        g_server.cache = NULL;
    }

    log_info("Server cleanup completed successfully");
}

// === MAIN SERVER FUNCTION ===

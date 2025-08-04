/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 18/07/25
 * License: MIT
 */
 
#ifndef SERVER_TCP_H
#define SERVER_TCP_H

// Constants
#define MAX_COMMAND_SIZE (BUFFER_SIZE * 4)
#define CLIENT_ID_SIZE 64
#define MAX_ERROR_MSG 256

#include <signal.h>
#include <netinet/in.h>

#include "pod_cache.h"
#include "resp_parser.h"

#define BUFFER_SIZE         4096
#define MAX_PENDING_CONNS   128
#define DEFAULT_PORT        6379
#define MAX_LINE_LENGTH     1024

/* Client connection context */
typedef struct {
    int socket;
    struct sockaddr_in addr;
    pthread_t thread_id;
    char client_id[64];
} client_ctx_t;

typedef struct {
    client_ctx_t *client_ctx;
    pod_cache_t *cache;
} server_thread_params_t;

// Global server state
typedef struct {
    volatile sig_atomic_t running;
    int socket_fd;
    pod_cache_t *cache;
} server_state_t;

typedef struct {
    resp_command_e type;
    const char *name;
    int (*handler)(client_ctx_t *client, pod_cache_t *cache, resp_command_t *cmd);
} command_handler_t;

typedef struct {
    char buffer[MAX_COMMAND_SIZE];
    size_t used;
    size_t capacity;
} command_buffer_t;

int tcp_server_start(void);

#endif //SERVER_TCP_H

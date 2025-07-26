/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 18/07/25
 * License: MIT
 */
 
#ifndef SERVER_TCP_H
#define SERVER_TCP_H
#include <netinet/in.h>

#include "pod_cache.h"

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
    pod_cache_t *pod_cache;
} server_thread_p;

int tcp_server_start(void);

#endif //SERVER_TCP_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: AGPL 3
 */
 
#ifndef CACHE_H
#define CACHE_H
#include <stdbool.h>
#include <stddef.h>

#include "lru_cache.h"
#include <pthread.h>
#include "cas.h"

#define MB_TO_BYTES(mb) ((mb) * 1024 * 1024)
#define BYTES_TO_MB(bytes) ((double)(bytes) / (1024.0 * 1024.0))

typedef unsigned short u_short;

typedef struct pod_cache {
    size_t total_capacity;
    size_t partition_capacity;
    u_short partition_count;
    lru_cache_t **partitions;
    cas_registry_t *cas_registry;
} pod_cache_t;

pod_cache_t *pod_cache_create(size_t capacity, u_short partitions);
void pod_cache_destroy(pod_cache_t *pod_cache);
int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size);
int pod_cache_get(pod_cache_t *cache, const char *key, void **out_value, size_t *out_value_size);
int pod_cache_evict(pod_cache_t *cache, const char *key);

#endif //CACHE_H

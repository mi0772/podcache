/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/pod_cache.h"

#include <stddef.h>
#include <stdlib.h>

#include "clogger.h"

pod_cache_t *pod_cache_create(size_t capacity, bool use_disk_cache) {
    pod_cache_t *pod_cache = malloc(sizeof(pod_cache_t));
    if (!pod_cache) return NULL;

    pod_cache->capacity = capacity;
    pod_cache->use_disk_cache = use_disk_cache;
    lru_cache_t *lru_cache = lru_cache_create(capacity);
    pod_cache->memory_cache = lru_cache;

    return pod_cache;
}

int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size) {
    // un elemento va sempre inserito nella cache in memory
    if (lru_cache_put(cache->memory_cache, key, value, value_size) != 0) {
        log_error("cannot put key %s in memory, errno = %d", key, -1);
        return -1;
    }
    log_info("key %s inserted into cache", key);
    return 0;
}

int pod_cache_get(pod_cache_t *cache, const char *key, void *out_value, size_t *out_value_size) {
    if (!cache) return -1;

    if (lru_cache_get(cache->memory_cache, key, out_value, out_value_size) != 0) {
        log_error("cannot get key %s from cache, errno = %d", key, -1);
        return -1;
    }
    return 0;
}


void pod_cache_destroy(pod_cache_t *pod_cache) {
    if (!pod_cache) return;

    lru_cache_destroy(pod_cache->memory_cache);
    free(pod_cache);
}
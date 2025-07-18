/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/pod_cache.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#include "cas.h"
#include "clogger.h"
#include <pthread.h>

pthread_mutex_t m_write_lock;

pod_cache_t *pod_cache_create(size_t capacity) {
    pod_cache_t *pod_cache = malloc(sizeof(pod_cache_t));
    if (!pod_cache) return NULL;

    pod_cache->capacity = capacity;
    lru_cache_t *lru_cache = lru_cache_create(capacity);
    pod_cache->memory_cache = lru_cache;
    if (pthread_mutex_init(&m_write_lock, NULL) != 0) {
        log_fatal("pthread_mutex_init failed");
        return NULL;
    }
    return pod_cache;
}

int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size) {
    // un elemento va sempre inserito nella cache in memory
    pthread_mutex_lock(&cache->mutex);
    if (lru_cache_put(cache->memory_cache, key, value, value_size) != 0) {
        pthread_mutex_unlock(&cache->mutex);
        log_error("cannot put key %s in memory, errno = %d", key, -1);
        return -1;
    }
    pthread_mutex_unlock(&cache->mutex);
    log_info("key %s inserted into cache", key);
    return 0;
}

int pod_cache_get(pod_cache_t *cache, const char *key, void **out_value, size_t *out_value_size) {
    if (!cache) return -1;

    int o_res = lru_cache_get(cache->memory_cache, key, out_value, out_value_size);

    switch (o_res) {
        case -1:
            log_error("cannot get key %s from cache, errno = %d", key, -1);
            return -1;
        case -100:
            log_debug("key %s not found, searching into disk cache", key);
            if (cas_get(key, out_value, out_value_size) == 0) {
                // trovato su disco, sposto nella cache in-memory
                lru_cache_put(cache->memory_cache, key, *out_value, *out_value_size);

                // rimuovo da disk cache
                cas_evict(key);

                return 0;
            }
            log_error("cannot get key %s from disk cache, errno = %d", key, -1);
            return -1;
        default:
            log_info("key %s found", key);
    }

    return 0;
}


void pod_cache_destroy(pod_cache_t *pod_cache) {
    if (!pod_cache) return;

    lru_cache_destroy(pod_cache->memory_cache);
    free(pod_cache);
}
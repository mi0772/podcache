/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */
 
#ifndef CACHE_H
#define CACHE_H
#include <stdbool.h>
#include <stddef.h>

#include "lru_cache.h"

#define MB_TO_BYTES(mb) ((mb) * 1024 * 1024)

typedef struct pod_cache {
    lru_cache_t *memory_cache;
    size_t capacity;
    bool use_disk_cache;
} pod_cache_t;

pod_cache_t *pod_cache_create(size_t capacity, bool use_disk_cache);
void pod_cache_destroy(pod_cache_t *pod_cache);
int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size);
int pod_cache_get(pod_cache_t *cache, const char *key, void **out_value, size_t *out_value_size);

#endif //CACHE_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */
 
#ifndef CACHE_H
#define CACHE_H
#include <stddef.h>

#define MB_TO_BYTES(mb) ((mb) * 1024 * 1024)

typedef struct cache cache_t;

// VT - Cache Operations
typedef struct cache_ops {
    int    (*put)       (cache_t *self, const char *key, void *value);
    void*  (*get)       (cache_t *self, const char *key);
    int    (*remove)    (cache_t *self, const char *key);
    void   (*destroy)   (cache_t *self);
    size_t (*size)      (cache_t *self);
    void   (*clear)     (cache_t *self);
} cache_ops_t;

struct cache {
    cache_ops_t* ops;  // "vtable"
    size_t max_size;
    size_t current_size;
    // ... altri campi comuni
};

// Factory functions per diversi tipi
cache_t* lru_cache_create(size_t max_size);

// API uniforme che usa la vtable
static int cache_put(cache_t *cache, const char* key, void* value) {
    return cache->ops->put(cache, key, value);
}

static void * cache_get(cache_t *cache, const char* key) {
    return cache->ops->get(cache, key);
}

static void cache_destroy(cache_t *cache) {
    cache->ops->destroy(cache);
}

static int cache_remove(cache_t *cache, const char *key) {
    return cache->ops->remove(cache, key);
}

static size_t cache_size(cache_t *cache) {
    return cache->ops->size(cache);
}

static void cache_clear(cache_t *cache) {
    cache->ops->clear(cache);
}

#endif //CACHE_H

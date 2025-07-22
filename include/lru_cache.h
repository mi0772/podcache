/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */
 
#ifndef LRU_CACHE_H
#define LRU_CACHE_H
#include <stddef.h>
#include <pthread.h>

typedef struct lru_node {
    char *key;
    void *value;
    size_t size;
    time_t creation_time;
    struct lru_node *next;
    struct lru_node *prev;

} lru_node_t;

typedef struct hash_node {
    char *key;
    lru_node_t *node;
    struct hash_node *next;
} hash_node_t;

// Cache LRU
typedef struct lru_cache {
    lru_node_t *head;
    lru_node_t *tail;
    hash_node_t **buckets;
    size_t max_bytes_capacity;
    size_t current_bytes_size;
    size_t hash_table_size;
    pthread_mutex_t mutex;
} lru_cache_t;

lru_cache_t *lru_cache_create(size_t max_bytes_capacity);
int lru_cache_put(lru_cache_t *cache, const char *key, void *value, size_t value_size);
int lru_cache_get(lru_cache_t *cache, const char *key, void **value, size_t *value_size);
void lru_cache_destroy(lru_cache_t *cache);
lru_node_t *lru_cache_get_tail_node(lru_cache_t *cache);
int lru_cache_remove_tail(lru_cache_t *cache);
#endif //LRU_CACHE_H

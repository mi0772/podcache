/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */
 
#ifndef LRU_CACHE_H
#define LRU_CACHE_H
#include <stddef.h>
#include <time.h>
#include "hash_table.h"

// nodo hash_table
typedef struct lru_node {
    char *key;
    void *value;
    size_t size;
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
} lru_cache_t;

/* ======================================================
 * forward declaration static functions
*  ====================================================== */
static lru_node_t *create_node(const char *key, size_t value_size, void *value);
static hash_node_t *create_hash_node(const char *key, lru_node_t *lru_node);
static void add_to_head(lru_cache_t *cache, lru_node_t *lru_node);
static void move_to_head(lru_cache_t *cache, lru_node_t *lru_node);
static void move_tail_to_disk(lru_cache_t *cache);

/* ======================================================
 * forward declaration public functions
*  ====================================================== */
lru_cache_t *lru_cache_create(size_t capacity);
int lru_cache_put(lru_cache_t *cache, const char *key, void *value, size_t value_size);
int lru_cache_get(lru_cache_t *cache, const char *key, void **value, size_t *value_size);
void lru_cache_destroy(lru_cache_t *cache);


#endif //LRU_CACHE_H

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
typedef struct LRUNode {
    char *key;
    void *value;
    size_t size;
    struct LRUNode *next;
    struct LRUNode *prev;

} LRUNode;

typedef struct HashNode {
    char *key;
    LRUNode *node;
    struct HashNode *next;
} HashNode;

// Cache LRU
typedef struct LRUCache {
    LRUNode *head;
    LRUNode *tail;
    HashNode **buckets;
    size_t capacity;
    size_t size;
    size_t hash_size;
} LRUCache;

/* ======================================================
 * forward declaration static functions
*  ====================================================== */
static LRUNode *create_node(const char *key, size_t value_size, void *value);
static HashNode *create_hash_node(const char *key, LRUNode *lru_node);
static void add_to_head(LRUCache *cache, LRUNode *lru_node);
static void move_to_head(LRUCache *cache, LRUNode *lru_node);

/* ======================================================
 * forward declaration public functions
*  ====================================================== */
LRUCache *LRUCache_create(size_t capacity);
int LRUCache_put(LRUCache *cache, const char *key, void *value, size_t value_size);
void LRUCache_destroy(LRUCache *cache);


#endif //LRU_CACHE_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "lru_cache.h"
#include <_string.h>
#include <stdlib.h>
#include "cache.h"
#include "hash_func.h"

/* =============================================
 * public functions implementation
 * ============================================= */
LRUCache *LRUCache_create(size_t capacity) {
    LRUCache *cache = calloc(1, sizeof(LRUCache));
    cache->buckets = calloc(capacity * 2, sizeof(HashNode *));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }

    cache->capacity = capacity;
    cache->hash_size = capacity * 2;
    cache->head = NULL;
    cache->tail = NULL;
    return cache;
}

int LRUCache_put(LRUCache *cache, const char *key, void *value, size_t value_size) {
    if (!cache) return -1;

    uint32_t hash = hash_key(key, cache->hash_size);

    HashNode *current = cache->buckets[hash];
    while (current) {
        if (strcmp(current->key, key) == 0) {

            free(current->node->value);
            current->node->value = malloc(value_size);
            memcpy(current->node->value, value, value_size);
            move_to_head(cache, current->node);
            return 0;
        }
        current = current->next;
    }

    //TODO: Gestire memoria piena e spostamento in head

    LRUNode *new_lru_node = create_node(key, value_size, value);
    HashNode *new_hash_node = create_hash_node(key, new_lru_node);
    new_hash_node->next = cache->buckets[hash];
    cache->buckets[hash] = new_hash_node;
    cache->size++;
    add_to_head(cache, new_lru_node);
    return 0;
}

void LRUCache_destroy(LRUCache *cache) {
    if (cache == NULL) return;

    LRUNode *current = cache->head;
    while (current) {
        LRUNode *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }

    for (int i=0 ; i < cache->hash_size ; i++) {
        while (cache->buckets[i]) {
            HashNode *node = cache->buckets[i]->next;
            free(cache->buckets[i]->key);
            free(cache->buckets[i]);
            cache->buckets[i] = node;
        }

    }
    free(cache->buckets);
    free(cache);
}

/* =============================================
 * static functions implementation
 * ============================================= */

static LRUNode *create_node(const char *key, size_t value_size, void *value) {
    LRUNode *new_lru_node = calloc(1, sizeof(LRUNode));
    new_lru_node->key = strdup(key);
    new_lru_node->value = malloc(value_size+1);
    memcpy(new_lru_node->value, value, value_size);
    new_lru_node->size = value_size;
    new_lru_node->next = NULL;
    return new_lru_node;
}

static HashNode *create_hash_node(const char *key, LRUNode *lru_node) {
    HashNode *new_hash_node = calloc(1, sizeof(HashNode));
    new_hash_node->key = strdup(key);
    new_hash_node->node = lru_node;
    new_hash_node->next = NULL;
    return new_hash_node;
}

static void add_to_head(LRUCache *cache, LRUNode *lru_node) {
    if (!cache || !lru_node) return;

    lru_node->prev = NULL;
    if (!cache->head) {
        cache->head = lru_node;
        cache->tail = lru_node;
        lru_node->next = NULL;
        return;
    }

    lru_node->next = cache->head;
    cache->head->prev = lru_node;
    cache->head = lru_node;
}

static void move_to_head(LRUCache *cache, LRUNode *lru_node) {
    if (!cache || !lru_node) return;

    // caso in cui sono già in testa, non faccio nulla
    if (cache->head == lru_node) return;

    //caso in cui sono in coda
    if (cache->tail == lru_node) {
        lru_node->prev->next = NULL;
        cache->tail = lru_node->prev;
        lru_node->prev = NULL;
        lru_node->next = cache->head;
        if (cache->head)
            cache->head->prev = lru_node;
        cache->head = lru_node;
        return;
    }

    // caso in cui sono nel mezzo
    lru_node->prev->next = lru_node->next;
    lru_node->next->prev = lru_node->prev;
    lru_node->prev = NULL;
    lru_node->next = cache->head;
    if (cache->head)
        cache->head->prev = lru_node;

    cache->head = lru_node;
}
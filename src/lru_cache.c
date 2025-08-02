/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/lru_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/clogger.h"
#include "../include/hash_func.h"

/* ======================================================
 * forward declaration static functions
 *  ====================================================== */
static lru_node_t *create_node(const char *key, size_t value_size, void *value);
static hash_node_t *create_hash_node(const char *key, lru_node_t *lru_node);
static void add_to_head(lru_cache_t *cache, lru_node_t *lru_node);
static void move_to_head(lru_cache_t *cache, lru_node_t *lru_node);
static size_t calculate_hash_table_size(size_t max_bytes_capacity);

/* =============================================
 * public functions implementation
 * ============================================= */
lru_cache_t *lru_cache_create(size_t max_bytes_capacity) {
    lru_cache_t *cache = calloc(1, sizeof(lru_cache_t));

    // create mutex for put
    if (pthread_mutex_init(&cache->mutex, NULL) != 0) {
        return NULL;
    }
    size_t estimated_capacity = calculate_hash_table_size(max_bytes_capacity) + 1;

    cache->buckets = calloc(estimated_capacity, sizeof(hash_node_t *));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }

    cache->max_bytes_capacity = max_bytes_capacity;
    cache->hash_table_size = estimated_capacity;
    cache->head = NULL;
    cache->tail = NULL;
    return cache;
}

int lru_cache_get(lru_cache_t *cache, const char *key, void **value, size_t *value_size) {
    if (!cache) return -1;

    uint32_t hash = hash_key(key, cache->hash_table_size);
    hash_node_t *current = cache->buckets[hash];
    while (current) {
        if (strcmp(current->key, key) == 0) {
            // item found, read value and move it to the head of linkedlist
            *value = malloc(current->node->size);
            if (!*value) {
                return -1; // Errore di allocazione
            }
            *value_size = current->node->size;
            memcpy(*value, current->node->value, current->node->size);
            log_debug("GET: retrieved '%.*s' (size: %zu)", (int)*value_size, (char *)*value,
                      *value_size);

            move_to_head(cache, current->node);
            return 0;
        }
        current = current->next;
    }
    // not found, returning -100
    return -100;
}

int lru_cache_evict(lru_cache_t *cache, const char *key) {
    if (!cache) return -1;

    uint32_t hash = hash_key(key, cache->hash_table_size);
    hash_node_t *current = cache->buckets[hash];
    hash_node_t *prev_hash = NULL;

    // Cerca nella hash table
    while (current) {
        if (strcmp(current->key, key) == 0) {
            lru_node_t *node_to_remove = current->node;

            // rimozione hash dall'hash table
            if (prev_hash) {
                prev_hash->next = current->next;
            } else {
                cache->buckets[hash] = current->next;
            }

            // rimozione dalla linkedlist lru
            if (cache->head == cache->tail) {
                // Caso 1: Unico elemento nella cache
                cache->head = NULL;
                cache->tail = NULL;
            } else if (node_to_remove == cache->head) {
                // elemento di test
                cache->head = node_to_remove->next;
                cache->head->prev = NULL;
            } else if (node_to_remove == cache->tail) {
                // elemento di coda
                cache->tail = node_to_remove->prev;
                cache->tail->next = NULL;
            } else {
                // elemento non testa e non coda
                node_to_remove->prev->next = node_to_remove->next;
                node_to_remove->next->prev = node_to_remove->prev;
            }

            // update contatore cache size
            cache->current_bytes_size -= node_to_remove->size;

            // memory free
            free(current->key);
            free(current);
            free(node_to_remove->key);
            free(node_to_remove->value);
            free(node_to_remove);

            log_debug("EVICT: removed key '%s'", key);
            return 0;
        }
        prev_hash = current;
        current = current->next;
    }

    // elemento non trovato
    return -100;
}

int lru_cache_put(lru_cache_t *cache, const char *key, void *value, size_t value_size) {
    if (!cache) return -1;

    pthread_mutex_lock(&cache->mutex);

    // Controllo se la memoria è disponibile
    if ((cache->current_bytes_size + value_size) >= cache->max_bytes_capacity) {
        // Memoria piena, ritorna codice speciale ma mantiene il lock
        log_info("memory full, move tail element into disk cache");
        pthread_mutex_unlock(&cache->mutex);
        return -900;
    }

    uint32_t hash = hash_key(key, cache->hash_table_size);

    hash_node_t *current = cache->buckets[hash];
    while (current) {
        if (strcmp(current->key, key) == 0) {

            free(current->node->value);
            size_t old_value_size = current->node->size;

            current->node->value = malloc(value_size);
            if (!current->node->value) {
                pthread_mutex_unlock(&cache->mutex);
                return -1; // Errore di allocazione
            }
            current->node->size = value_size;
            memcpy(current->node->value, value, value_size);
            log_debug("PUT: updating '%.*s' (size: %zu)", (int)value_size, (char *)value,
                      value_size);

            cache->current_bytes_size += (current->node->size - old_value_size);
            // campo aggiornato, va spostato in head
            move_to_head(cache, current->node);
            pthread_mutex_unlock(&cache->mutex);
            return 0;
        }
        current = current->next;
    }

    lru_node_t *new_lru_node = create_node(key, value_size, value);
    if (!new_lru_node) {
        pthread_mutex_unlock(&cache->mutex);
        return -1;
    }

    hash_node_t *new_hash_node = create_hash_node(key, new_lru_node);
    if (!new_hash_node) {
        free(new_lru_node->key);
        free(new_lru_node->value);
        free(new_lru_node);
        pthread_mutex_unlock(&cache->mutex);
        return -1;
    }
    new_hash_node->next = cache->buckets[hash];
    cache->buckets[hash] = new_hash_node;

    cache->current_bytes_size += value_size;
    add_to_head(cache, new_lru_node);
    pthread_mutex_unlock(&cache->mutex);
    return 0;
}

void lru_cache_destroy(lru_cache_t *cache) {
    if (cache == NULL) return;

    // Libera tutti i nodi LRU attraversando solo la lista LRU
    lru_node_t *current = cache->head;
    while (current) {
        lru_node_t *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }

    // Libera i nodi hash dall'hash table
    for (size_t i = 0; i < cache->hash_table_size; i++) {
        hash_node_t *hash_node = cache->buckets[i];
        while (hash_node) {
            hash_node_t *next = hash_node->next;
            free(hash_node->key);
            free(hash_node);
            hash_node = next;
        }
    }

    // Cleanup mutex
    pthread_mutex_destroy(&cache->mutex);

    // Libera l'array di bucket e la cache
    free(cache->buckets);
    free(cache);
}

/* =============================================
 * static functions implementation
 * ============================================= */

static lru_node_t *create_node(const char *key, size_t value_size, void *value) {
    lru_node_t *new_lru_node = calloc(1, sizeof(lru_node_t));
    if (!new_lru_node) return NULL;

    new_lru_node->key = strdup(key);
    if (!new_lru_node->key) {
        free(new_lru_node);
        return NULL;
    }

    new_lru_node->value = malloc(value_size); // Rimosso +1 non necessario
    if (!new_lru_node->value) {
        free(new_lru_node->key);
        free(new_lru_node);
        return NULL;
    }

    memcpy(new_lru_node->value, value, value_size);
    log_debug("PUT: storing '%.*s' (size: %zu)", (int)value_size, (char *)value, value_size);
    new_lru_node->size = value_size;
    new_lru_node->next = NULL;
    new_lru_node->creation_time = time(NULL);

    return new_lru_node;
}

static hash_node_t *create_hash_node(const char *key, lru_node_t *lru_node) {
    hash_node_t *new_hash_node = calloc(1, sizeof(hash_node_t));
    if (!new_hash_node) return NULL;

    new_hash_node->key = strdup(key);
    if (!new_hash_node->key) {
        free(new_hash_node);
        return NULL;
    }

    new_hash_node->node = lru_node;
    new_hash_node->next = NULL;
    return new_hash_node;
}

static void add_to_head(lru_cache_t *cache, lru_node_t *lru_node) {
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

lru_node_t *lru_cache_get_tail_node(lru_cache_t *cache) {
    if (!cache->tail) return NULL;

    return cache->tail;
}

int lru_cache_remove_tail(lru_cache_t *cache) {
    if (!cache || !cache->tail) return -1;

    lru_node_t *tail_node = cache->tail;

    if (cache->head == cache->tail) {
        cache->head = NULL;
        cache->tail = NULL;
    } else {
        cache->tail = tail_node->prev;
        cache->tail->next = NULL;
    }

    uint32_t index = hash_key(tail_node->key, cache->hash_table_size);
    hash_node_t *current = cache->buckets[index];
    hash_node_t *prev = NULL;

    while (current) {
        if (current->node == tail_node) {
            if (prev) {
                prev->next = current->next;
            } else {
                cache->buckets[index] = current->next;
            }
            free(current->key);
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }
    cache->current_bytes_size -= tail_node->size;
    free(tail_node->key);
    free(tail_node->value);
    free(tail_node);
    log_info("removed tail element from list");
    return 0;
}

static void move_to_head(lru_cache_t *cache, lru_node_t *lru_node) {
    if (!cache || !lru_node) return;

    // caso in cui sono già in testa, non faccio nulla
    if (cache->head == lru_node) return;

    // caso in cui sono l'unico elemento
    if (lru_node->prev == NULL && lru_node->next == NULL) return;

    // caso in cui sono in coda
    if (cache->tail == lru_node) {
        lru_node->prev->next = NULL;
        cache->tail = lru_node->prev;
        lru_node->prev = NULL;
        lru_node->next = cache->head;
        if (cache->head) cache->head->prev = lru_node;
        cache->head = lru_node;
        return;
    }

    // caso in cui sono nel mezzo
    lru_node->prev->next = lru_node->next;
    lru_node->next->prev = lru_node->prev;
    lru_node->prev = NULL;
    lru_node->next = cache->head;
    if (cache->head) cache->head->prev = lru_node;

    cache->head = lru_node;
}

static size_t calculate_hash_table_size(size_t max_bytes_capacity) {
    // Stima elementi medi (assumendo 1KB per elemento)
    size_t estimated_elements = max_bytes_capacity / 1024;

    // Load factor target: 0.75 (75% di riempimento)
    size_t target_size = estimated_elements / 0.75;

    // Arrotonda alla prossima potenza di 2 per performance
    size_t size = 16;                            // minimo
    while (size < target_size && size < 65536) { // max 64K
        size <<= 1;
    }

    return size;
}
/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/pod_cache.h"

#include <stddef.h>
#include <stdlib.h>

#include "../include/cas.h"
#include "../include/clogger.h"
#include <pthread.h>

#include "../include/hash_func.h"

#define MAX_PARTITIONS 20

static int get_partition(uint32_t hash, u_short partition_count);

pod_cache_t *pod_cache_create(size_t capacity, u_short partitions) {
    pod_cache_t *pod_cache = malloc(sizeof(pod_cache_t));
    if (!pod_cache) return NULL;

    size_t single_partition_capacity = capacity / partitions;

    pod_cache->partition_capacity = single_partition_capacity;
    pod_cache->partition_count = partitions;
    pod_cache->total_capacity = capacity;
    pod_cache->cas_registry = cas_create_registry();

    pod_cache->partitions = malloc(partitions * sizeof(lru_cache_t *));
    for (int i = 0; i < partitions; i++) {
        pod_cache->partitions[i] = lru_cache_create(single_partition_capacity);
    }

    return pod_cache;
}

int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size) {
    // un elemento va sempre inserito nella cache in memory
    int partition_index = get_partition(hash(key), cache->partition_count);

    int put_response = lru_cache_put(cache->partitions[partition_index], key, value, value_size);
    switch (put_response) {
    case -1:
        log_error("cannot put key %s in memory, errno = %d", key, -1);
        return -1;
    case -900:
        log_warn("partition %d full, move tail to disk", partition_index);
        // get pointer to tail element in partition
        lru_node_t *tail = lru_cache_get_tail_node(cache->partitions[partition_index]);

        // write it to disk cache
        char output_path[512];
        cas_put(cache->cas_registry, tail->key, tail->value, tail->size, output_path);
        // TODO: inserire nel registry l'output generato, fare una routine perchè è un array
        // dinamico, se non c'è spazio bisogna fare realloc
        cas_add_to_registry(cache->cas_registry, output_path);

        // remove from tail
        lru_cache_remove_tail(cache->partitions[partition_index]);
        return 0;
    default:
        return partition_index;
    }
}

int pod_cache_get(pod_cache_t *cache, const char *key, void **out_value, size_t *out_value_size) {
    if (!cache) return -1;

    int partition_index = get_partition(hash(key), cache->partition_count);
    int o_res = lru_cache_get(cache->partitions[partition_index], key, out_value, out_value_size);

    switch (o_res) {
    case -1:
        log_error("cannot get key %s from cache, errno = %d", key, -1);
        return -1;
    case -100:
        log_debug("key %s not found, searching into disk cache", key);
        if (cas_get(cache->cas_registry, key, out_value, out_value_size) == 0) {
            // trovato su disco, sposto nella cache in-memory
            lru_cache_put(cache->partitions[partition_index], key, *out_value, *out_value_size);

            // rimuovo da disk cache
            cas_evict(key, cache->cas_registry);

            return 0;
        }
        log_error("cannot get key %s from disk cache, errno = %d", key, -1);
        return -1;
    default:
        log_info("key %s found", key);
    }

    return partition_index;
}

int pod_cache_evict(pod_cache_t *cache, const char *key) {
    if (!cache) return -1;

    int partition_index = get_partition(hash(key), cache->partition_count);
    int memory_evict_result = lru_cache_evict(cache->partitions[partition_index], key);
    if (memory_evict_result == -100) {
        log_debug("%s not found in memory cache, try to remove it from cas", key);
        // non trovata in memoria, provo a cancellarla da filesystem
        int cas_evict_result = cas_evict(key, cache->cas_registry);
        if (cas_evict_result == 0) {
            log_info("%s was removed from cas");
            return 1;
        }
        if (cas_evict_result == -1) {
            log_warn("%s was not present into cas");
            return 0;
        }
    }
    if (memory_evict_result == 0) {
        log_info("%s was removed from memory cache", key);
        return 1;
    }
    return 0;
}

void pod_cache_destroy(pod_cache_t *pod_cache) {
    if (!pod_cache) return;

    cas_registry_destroy(pod_cache->cas_registry);

    if (pod_cache->partitions) {
        for (int i = 0; i < pod_cache->partition_count; i++) {
            lru_cache_destroy(pod_cache->partitions[i]);
        }
        free(pod_cache->partitions); // Libera l'array delle partizioni
    }

    free(pod_cache); // Libera la struttura principale alla fine
}

static int get_partition(uint32_t hash, u_short partition_count) { return hash % partition_count; }
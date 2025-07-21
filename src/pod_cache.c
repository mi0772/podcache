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

#include <pthread.h>
#include "cas.h"
#include "clogger.h"

#include "hash_func.h"

#define MAX_PARTITIONS 20

static int get_partition(uint32_t hash, u_short partition_count);

pod_cache_t *pod_cache_create(size_t capacity, u_short partitions) {
    pod_cache_t *pod_cache = malloc(sizeof(pod_cache_t));
    if (!pod_cache) return NULL;

    size_t single_partition_capacity = capacity / partitions;

    pod_cache->partition_capacity = single_partition_capacity;
    pod_cache->partition_number = partitions;
    pod_cache->total_capacity = capacity;

    pod_cache->partitions = malloc( partitions * sizeof(lru_cache_t *));
    for (int i=0 ; i < partitions ; i++) {
        pod_cache->partitions[i] = lru_cache_create(single_partition_capacity);
    }

    return pod_cache;
}

int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size) {
    // un elemento va sempre inserito nella cache in memory
    int partition_index = get_partition(hash(key), cache->partition_number);

    if (lru_cache_put(cache->partitions[partition_index], key, value, value_size) != 0) {
        log_error("cannot put key %s in memory, errno = %d", key, -1);
        return -1;
    }
    log_info("key %s inserted into cache", key);
    return partition_index;
}

int pod_cache_get(pod_cache_t *cache, const char *key, void **out_value, size_t *out_value_size) {
    if (!cache) return -1;

    int partition_index = get_partition(hash(key), cache->partition_number);
    int o_res = lru_cache_get(cache->partitions[partition_index], key, out_value, out_value_size);

    switch (o_res) {
        case -1:
            log_error("cannot get key %s from cache, errno = %d", key, -1);
            return -1;
        case -100:
            log_debug("key %s not found, searching into disk cache", key);
            if (cas_get(key, out_value, out_value_size) == 0) {
                // trovato su disco, sposto nella cache in-memory
                lru_cache_put(cache->partitions[partition_index], key, *out_value, *out_value_size);

                // rimuovo da disk cache
                cas_evict(key);

                return 0;
            }
            log_error("cannot get key %s from disk cache, errno = %d", key, -1);
            return -1;
        default:
            log_info("key %s found", key);
    }

    return partition_index;
}


void pod_cache_destroy(pod_cache_t *pod_cache) {
    if (!pod_cache) return;

    for (int i=0 ; i < pod_cache->partition_number ; i++) {
        lru_cache_destroy(pod_cache->partitions[i]);
        free(pod_cache);
    }

    //TODO: Cleanup
    // vanno rimossi tutti i file creati ? credo di si anche se non abbiamo un registro di quali sono i suoi
    // questo potrebbe portare a cancellare anche elementi di altre cache

}

static int get_partition(uint32_t hash, u_short partition_count) {
    return hash % partition_count;
}
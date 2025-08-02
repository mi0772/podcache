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
    log_debug("Creating pod cache with capacity %zu bytes, %d partitions", capacity, partitions);

    pod_cache_t *pod_cache = malloc(sizeof(pod_cache_t));
    if (!pod_cache) {
        log_error("Failed to allocate memory for pod_cache_t structure");
        return NULL;
    }

    size_t single_partition_capacity = capacity / partitions;
    log_debug("Each partition will have capacity: %zu bytes", single_partition_capacity);

    pod_cache->partition_capacity = single_partition_capacity;
    pod_cache->partition_count = partitions;
    pod_cache->total_capacity = capacity;
    pod_cache->cas_registry = cas_create_registry();

    if (!pod_cache->cas_registry) {
        log_error("Failed to create CAS registry");
        free(pod_cache);
        return NULL;
    }
    log_debug("CAS registry created successfully");

    pod_cache->partitions = malloc(partitions * sizeof(lru_cache_t *));
    if (!pod_cache->partitions) {
        log_error("Failed to allocate memory for partitions array");
        cas_registry_destroy(pod_cache->cas_registry);
        free(pod_cache);
        return NULL;
    }

    for (int i = 0; i < partitions; i++) {
        pod_cache->partitions[i] = lru_cache_create(single_partition_capacity);
        if (!pod_cache->partitions[i]) {
            log_error("Failed to create partition %d", i);
            // Cleanup already created partitions
            for (int j = 0; j < i; j++) {
                lru_cache_destroy(pod_cache->partitions[j]);
            }
            free(pod_cache->partitions);
            cas_registry_destroy(pod_cache->cas_registry);
            free(pod_cache);
            return NULL;
        }
        log_debug("Created partition %d with capacity %zu bytes", i, single_partition_capacity);
    }

    log_info("Pod cache created successfully: %d partitions, %zu total bytes", partitions,
             capacity);
    return pod_cache;
}

int pod_cache_put(pod_cache_t *cache, const char *key, void *value, size_t value_size) {
    if (!cache || !key || !value) {
        log_error("Invalid parameters in pod_cache_put: cache=%p, key=%p, value=%p", (void *)cache,
                  (void *)key, (void *)value);
        return -1;
    }

    log_debug("PUT operation: key='%s', value_size=%zu", key, value_size);

    // un elemento va sempre inserito nella cache in memory
    int partition_index = get_partition(hash(key), cache->partition_count);
    log_debug("Selected partition %d for key '%s'", partition_index, key);

    int put_response = lru_cache_put(cache->partitions[partition_index], key, value, value_size);
    switch (put_response) {
    case -1:
        log_error("Failed to put key '%s' in memory partition %d", key, partition_index);
        return -1;
    case -900:
        log_info("Partition %d full, moving tail element to disk storage", partition_index);
        // get pointer to tail element in partition
        lru_node_t *tail = lru_cache_get_tail_node(cache->partitions[partition_index]);
        if (!tail) {
            log_error("No tail element found in partition %d", partition_index);
            return -1;
        }

        log_debug("Moving key '%s' from memory to disk", tail->key);

        // write it to disk cache
        char output_path[512];
        int cas_result =
            cas_put(cache->cas_registry, tail->key, tail->value, tail->size, output_path);
        if (cas_result != 0) {
            log_error("Failed to write key '%s' to disk storage, error: %d", tail->key, cas_result);
            return -1;
        }

        log_debug("Successfully wrote key '%s' to disk at path: %s", tail->key, output_path);

        // TODO: inserire nel registry l'output generato, fare una routine perchè è un array
        // dinamico, se non c'è spazio bisogna fare realloc
        cas_add_to_registry(cache->cas_registry, output_path);

        // remove from tail
        lru_cache_remove_tail(cache->partitions[partition_index]);
        log_debug("Removed tail element from memory partition %d", partition_index);

        // Now retry the put operation
        put_response = lru_cache_put(cache->partitions[partition_index], key, value, value_size);
        if (put_response < 0) {
            log_error("Failed to put key '%s' after freeing space in partition %d", key,
                      partition_index);
            return -1;
        }
        log_info("Successfully stored key '%s' in partition %d after disk eviction", key,
                 partition_index);
        return partition_index;
    default:
        log_debug("Successfully stored key '%s' in partition %d", key, partition_index);
        return partition_index;
    }
}

int pod_cache_get(pod_cache_t *cache, const char *key, void **out_value, size_t *out_value_size) {
    if (!cache || !key || !out_value || !out_value_size) {
        log_error("Invalid parameters in pod_cache_get: cache=%p, key=%p, out_value=%p, "
                  "out_value_size=%p",
                  (void *)cache, (void *)key, (void *)out_value, (void *)out_value_size);
        return -1;
    }

    log_debug("GET operation: key='%s'", key);

    int partition_index = get_partition(hash(key), cache->partition_count);
    log_debug("Searching in partition %d for key '%s'", partition_index, key);

    int o_res = lru_cache_get(cache->partitions[partition_index], key, out_value, out_value_size);

    switch (o_res) {
    case -1:
        log_error("Memory allocation error while getting key '%s' from partition %d", key,
                  partition_index);
        return -1;
    case -100:
        log_debug("Key '%s' not found in memory partition %d, searching in disk storage", key,
                  partition_index);
        if (cas_get(cache->cas_registry, key, out_value, out_value_size) == 0) {
            log_info("Key '%s' found in disk storage, promoting to memory", key);

            // trovato su disco, sposto nella cache in-memory
            int put_result =
                lru_cache_put(cache->partitions[partition_index], key, *out_value, *out_value_size);
            if (put_result < 0) {
                log_warn(
                    "Failed to promote key '%s' to memory partition %d, but returning disk value",
                    key, partition_index);
            } else {
                log_debug("Successfully promoted key '%s' to memory partition %d", key,
                          partition_index);
            }

            // rimuovo da disk cache
            if (cas_evict(key, cache->cas_registry) == 0) {
                log_debug("Successfully removed key '%s' from disk storage after promotion", key);
            } else {
                log_warn("Failed to remove key '%s' from disk storage after promotion", key);
            }

            return 0;
        }
        log_debug("Key '%s' not found in disk storage", key);
        return -1;
    default:
        log_debug("Key '%s' found in memory partition %d", key, partition_index);
    }

    return partition_index;
}

int pod_cache_evict(pod_cache_t *cache, const char *key) {
    if (!cache || !key) {
        log_error("Invalid parameters in pod_cache_evict: cache=%p, key=%p", (void *)cache,
                  (void *)key);
        return -1;
    }

    log_debug("EVICT operation: key='%s'", key);

    int partition_index = get_partition(hash(key), cache->partition_count);
    log_debug("Evicting from partition %d for key '%s'", partition_index, key);

    int memory_evict_result = lru_cache_evict(cache->partitions[partition_index], key);
    if (memory_evict_result == -100) {
        log_debug("Key '%s' not found in memory partition %d, attempting disk storage eviction",
                  key, partition_index);
        // non trovata in memoria, provo a cancellarla da filesystem
        int cas_evict_result = cas_evict(key, cache->cas_registry);
        if (cas_evict_result == 0) {
            log_info("Key '%s' successfully removed from disk storage", key);
            return 1;
        }
        if (cas_evict_result == -1) {
            log_debug("Key '%s' was not present in disk storage", key);
            return 0;
        }
    }
    if (memory_evict_result == 0) {
        log_info("Key '%s' successfully removed from memory partition %d", key, partition_index);
        return 1;
    }

    log_warn("Failed to evict key '%s' from both memory and disk storage", key);
    return 0;
}

void pod_cache_destroy(pod_cache_t *pod_cache) {
    if (!pod_cache) {
        log_warn("Attempted to destroy NULL pod_cache");
        return;
    }

    log_info("Destroying pod cache...");

    if (pod_cache->cas_registry) {
        log_debug("Destroying CAS registry");
        cas_registry_destroy(pod_cache->cas_registry);
    }

    if (pod_cache->partitions) {
        log_debug("Destroying %d partitions", pod_cache->partition_count);
        for (int i = 0; i < pod_cache->partition_count; i++) {
            if (pod_cache->partitions[i]) {
                log_debug("Destroying partition %d", i);
                lru_cache_destroy(pod_cache->partitions[i]);
            }
        }
        free(pod_cache->partitions); // Libera l'array delle partizioni
    }

    free(pod_cache); // Libera la struttura principale alla fine
    log_info("Pod cache destroyed successfully");
}

static int get_partition(uint32_t hash, u_short partition_count) { return hash % partition_count; }
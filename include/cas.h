/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 17/07/25
 * License: MIT
 */
 
#ifndef CAS_H
#define CAS_H
#include <stddef.h>

typedef struct cas_registry {
    char **entries;
    size_t entries_count;
    size_t capacity;
} cas_registry_t;

typedef struct fs_path {
    char *p[4];
} fs_path_t;


cas_registry_t *cas_create_registry();
int cas_put(const char *key, void *value, size_t value_size, char *output_path);
int cas_get(const char *key, void **buffer, size_t *actual_size);
int cas_evict(const char *key);


#endif //CAS_H

/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 17/07/25
 * License: MIT
 */
 
#ifndef CAS_H
#define CAS_H
#include <stddef.h>

typedef struct fs_path {
    char *p[4];
} fs_path_t;

int cas_put(const char *key, void *value, size_t value_size);
int cas_get(const char *key, void **buffer, size_t *actual_size);
int cas_evict(const char *key);

static int cas_remove(fs_path_t *fs_path);
static int cas_create_directory(const char *key, char *output_path);

static fs_path_t *create_fs_path(const char hash[65]);
static void substring(const char *str, int portion, char *output);
static int return_and_free(int result, fs_path_t *path);
static char *write_path(const fs_path_t *path);
static void free_path(fs_path_t *path);
#endif //CAS_H

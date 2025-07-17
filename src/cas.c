/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 17/07/25
 * License: MIT
 */

#define CHUNK_PATH 16

#include "../include/cas.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include "hash_func.h"

#define BASE_PATH "."

int cas_put(const char *key, void *value, size_t value_size) {
    char path[512];
    if (cas_create_directory(key, path) != 0) return -1;

    //ho il path, ci devo scrivere dentro il contenuto di value
    char complete_path[512];
    sprintf(complete_path, "%s/%s", path, "value.dat");
    FILE *fp = fopen(complete_path, "wb");
    if (!fp) return -1;
    if (fwrite(value, 1, value_size, fp) != value_size) {
        fclose(fp);
        return -9;
    }

    fclose(fp);
    return 0;
}

int cas_evict(const char *key) {
    char path[512];
    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);

    sprintf(path, "%s/%s/%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    struct stat st;
    if (stat(path, &st) == 0) {
        remove(path);
        free(fs_path);
        return 0;
    }
    return -1;
}

/* si usa con :
    void *data = NULL;           // Inizializza a NULL
    size_t size;
    int result = cas_get("my_key", &data, &size);
*/
int cas_get(const char *key, void **buffer, size_t *actual_size) {
    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);
    char *path = get_path(fs_path);
    char complete_path[512];
    sprintf(complete_path, "%s/%s", path, "value.dat");

    struct stat st;
    if (stat(complete_path, &st) != 0) {
        free_path(fs_path);
        free(path);
        return -1;
    }

    size_t file_size = st.st_size;
    *buffer = malloc(file_size);

    FILE *fp = fopen(complete_path, "rb");
    if (!fp) {
        free(*buffer);
        free_path(fs_path);
        free(path);
        return -1;
    }

    size_t read_size = fread(*buffer, 1, file_size, fp);
    if (read_size < file_size) {
        free(*buffer);
        free_path(fs_path);
        free(path);
        return -1;
    }

    *actual_size = read_size;
    fclose(fp);
    free_path(fs_path);
    free(path);
    return 0;
}

/* ========================================
 * static functions
 * ======================================== */

static int cas_create_directory(const char *key, char *output_path) {
    if (!key) return -1;

    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);

    char path[512];
    sprintf(path, "%s/%s/%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    struct stat st;
    if (stat(path, &st) == 0) {
        // entry esistente, la rimuovo
        if (cas_remove(fs_path) != 0) return return_and_free(-1, fs_path);
    }

    sprintf(path, "%s/%s", BASE_PATH, fs_path->p[0]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);

    sprintf(path, "%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);

    sprintf(path, "%s/%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);

    sprintf(path, "%s/%s/%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    if (mkdir(path, 0755) != 0) return return_and_free(-1, fs_path);

    strcpy(output_path, path);
    return return_and_free(0, fs_path);
}

static int cas_remove(fs_path_t *fs_path) {
    char path[512];

    sprintf(path, "%s/%s/%s/%s/%s/value.dat", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    remove(path);

    sprintf(path, "%s/%s/%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    if (remove(path) != 0) return -1;

    sprintf(path, "%s/%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1], fs_path->p[2]);
    if (remove(path) != 0) return -1;

    sprintf(path, "%s/%s/%s", BASE_PATH, fs_path->p[0], fs_path->p[1]);
    if (remove(path) != 0) return -1;

    sprintf(path, "%s/%s", BASE_PATH, fs_path->p[0]);
    if (remove(path) != 0) return -1;

    return 0;
}

static int return_and_free(int result, fs_path_t *path) {
    free_path(path);
    return result;
}
static void substring(const char *str, int portion, char *output) {
    int start_index = CHUNK_PATH * portion;
    strncpy(output, str + start_index, CHUNK_PATH);
    output[CHUNK_PATH] = '\0';
}

static fs_path_t *create_fs_path(const char hash[65]) {
    fs_path_t *r = malloc(sizeof(fs_path_t));

    for (int i=0 ; i < 4 ; i++) {
        r->p[i] = malloc(CHUNK_PATH + 1);
        substring(hash, i, r->p[i]);
    }
    return r;
}

static char *get_path(const fs_path_t *path) {
    char *r = calloc(1, 512);
    sprintf(r, "%s/%s/%s/%s/%s", BASE_PATH, path->p[0], path->p[1], path->p[2], path->p[3]);
    return r;
}

static void free_path(fs_path_t *path) {
    if (!path) return;

    free(path->p[0]);
    free(path->p[1]);
    free(path->p[2]);
    free(path->p[3]);
    free(path);
}
/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 17/07/25
 * License: MIT
 */

#define CHUNK_PATH 16

#include "../include/cas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#include "hash_func.h"


#define CAS_REGISTRY_INITIAL_CAPACITY 100

/* ========================================================
 * forward static declaration
 * ======================================================== */
static int cas_remove(const cas_registry_t *registry, fs_path_t *fs_path);
static int cas_create_directory(const cas_registry_t *registry, const char *key, char *output_path);
static fs_path_t *create_fs_path(const char hash[65]);
static void substring(const char *str, int portion, char *output);
static int return_and_free(int result, fs_path_t *path);
static char *get_path(const cas_registry_t *registry, const fs_path_t *path);
static void free_path(fs_path_t *path);
static void generate_base_path(char *buffer);
int cleanup(const char *path);

/* =============================================
 * public functions implementation
 * ============================================= */

cas_registry_t *cas_create_registry() {

    cas_registry_t *registry = malloc(sizeof(cas_registry_t));
    generate_base_path(registry->base_path);

    registry->entries = malloc(CAS_REGISTRY_INITIAL_CAPACITY * sizeof(char *));
    registry->capacity = CAS_REGISTRY_INITIAL_CAPACITY;
    registry->entries_count = 0;
    return registry;
}


int cas_put(const cas_registry_t *registry, const char *key, void *value, size_t value_size, char *output_path) {
    if (cas_create_directory(registry, key, output_path) != 0) return -1;

    //ho il path, ci devo scrivere dentro il contenuto di value
    char complete_path[512];
    sprintf(complete_path, "%s/%s", output_path, "value.dat");
    FILE *fp = fopen(complete_path, "wb");
    if (!fp) return -1;
    if (fwrite(value, 1, value_size, fp) != value_size) {
        fclose(fp);
        return -9;
    }
    fclose(fp);

    sprintf(complete_path, "%s/%s", output_path, "time.dat");
    fp = fopen(complete_path, "wb");
    if (!fp) return -1;
    time_t now = time(NULL);
    fprintf(fp, "%ld", (long)now);
    fclose(fp);
    return 0;
}

static int remove_dir(char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) {
        int remove_result = remove(dir);
        return remove_result;
    }
    return -1;
}

int cas_evict(const char *key, cas_registry_t *registry) {
    char hash[65] = {'\0'};
    char path[512] = {'\0'};

    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);

    // Array dei path da rimuovere (dal più profondo al più superficiale)
    const char *patterns[] = {
        "%s/%s/%s/%s/%s/value.dat",
        "%s/%s/%s/%s/%s",
        "%s/%s/%s/%s",
        "%s/%s/%s",
        "%s/%s"
    };

    int founded = 1;
    for (int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), patterns[i], registry->base_path,
                fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
        if (remove_dir(path) == -1) founded = 0;
    }
    if (founded == 0) {
        free(fs_path);
        return -1;
    }

    char p[512] = {'\0'};
    sprintf(p, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    printf("p = %s\n", p);
    for (int i=0 ; i < registry->entries_count ; i++) {
        if (strcmp(registry->entries[i], p) == 0) {
            for (int j = i ; j < registry->entries_count-1 ; j++) {
                registry->entries[j] = registry->entries[j+1];
            }
            free(registry->entries[registry->entries_count-1]);
            registry->entries_count--;
            break;
        }
    }

    free(fs_path);
    return 0;
}

int cas_add_to_registry(cas_registry_t *registry, char *path) {
    if (registry->entries_count + 1 >= registry->capacity) {
        registry->capacity *= 2;
        registry->entries = realloc(registry->entries, registry->capacity);
    }

    registry->entries[registry->entries_count] = malloc(sizeof(char) * strlen(path));
    strcpy(registry->entries[registry->entries_count], path);
    registry->entries_count++;
    return 0;
}

void cas_registry_destroy(cas_registry_t *registry) {
    if (!registry) return;

    if (registry->entries != NULL) {
        // Prima libera ogni singola stringa
        for (size_t i = 0; i < registry->entries_count; i++) {
            if (registry->entries[i] != NULL) {
                //TODO: rimuovi il file se esistente
                free(registry->entries[i]);  // libera la stringa
            }
        }
        cleanup(registry->base_path);
        // Poi libera l'array di puntatori
        free(registry->entries);
    }

    // Infine libera la struct
    free(registry);
}

/* si usa con :
    void *data = NULL;           // Inizializza a NULL
    size_t size;
    int result = cas_get("my_key", &data, &size);
*/
int cas_get(const cas_registry_t *registry, const char *key, void **buffer, size_t *actual_size) {
    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);
    char *path = get_path(registry, fs_path);
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

static int cas_create_directory(const cas_registry_t *registry, const char *key, char *output_path) {
    if (!key) return -1;

    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);

    char path[512] = {'\0'};
    sprintf(path, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    struct stat st;
    if (stat(path, &st) == 0) {
        // entry esistente, la rimuovo
        if (cas_remove(registry, fs_path) != 0) return return_and_free(-1, fs_path);
    }

    sprintf(path, "%s", registry->base_path);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);


    sprintf(path, "%s/%s", registry->base_path, fs_path->p[0]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);

    sprintf(path, "%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);

    sprintf(path, "%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return return_and_free(-1, fs_path);

    sprintf(path, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    if (mkdir(path, 0755) != 0) return return_and_free(-1, fs_path);

    strcpy(output_path, path);
    return return_and_free(0, fs_path);
}

static int cas_remove(const cas_registry_t *registry, fs_path_t *fs_path) {
    char path[512];

    sprintf(path, "%s/%s/%s/%s/%s/value.dat", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    remove(path);

    sprintf(path, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2], fs_path->p[3]);
    if (remove(path) != 0) return -1;

    sprintf(path, "%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2]);
    if (remove(path) != 0) return -1;

    sprintf(path, "%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1]);
    if (remove(path) != 0) return -1;

    sprintf(path, "%s/%s", registry->base_path, fs_path->p[0]);
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

static char *get_path(const cas_registry_t *registry, const fs_path_t *path) {
    char *r = calloc(1, 512);
    sprintf(r, "%s/%s/%s/%s/%s", registry->base_path, path->p[0], path->p[1], path->p[2], path->p[3]);
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

static void generate_base_path(char *buffer) {
    static int seeded = 0;
    char root_fs[512] = "./";
    char *value = getenv("PODCACHE_FSROOT");

    if (value != NULL) {
        strcpy(root_fs, value);
    }

    if (!seeded) {
        srand(time(NULL) ^ getpid());  // più entropia
        seeded = 1;
    }

    sprintf(buffer, "%s%08x",root_fs, (unsigned int)rand());
}

int cleanup(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char filepath[1024];

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Salta . e ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Costruisci il path completo
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        // Controlla se è file o directory
        if (stat(filepath, &statbuf) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // È una directory: chiamata ricorsiva
            cleanup(filepath);
        } else {
            // È un file: rimuovi
            if (remove(filepath) != 0) {
                perror("remove file");
            }
        }
    }

    closedir(dir);

    // Rimuovi la directory ora vuota
    if (rmdir(path) != 0) {
        perror("rmdir");
        return -1;
    }

    return 0;
}
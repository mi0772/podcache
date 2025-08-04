/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 17/07/25
 * License: MIT
 */

#define CHUNK_PATH 16

#include "../include/cas.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../include/clogger.h"
#include "../include/hash_func.h"

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
    log_debug("Creating CAS registry");

    cas_registry_t *registry = malloc(sizeof(cas_registry_t));
    if (!registry) {
        log_error("Failed to allocate memory for CAS registry");
        return NULL;
    }

    generate_base_path(registry->base_path);
    log_debug("CAS base path set to: %s", registry->base_path);

    registry->entries = malloc(CAS_REGISTRY_INITIAL_CAPACITY * sizeof(char *));
    if (!registry->entries) {
        log_error("Failed to allocate memory for CAS registry entries");
        free(registry);
        return NULL;
    }

    registry->capacity = CAS_REGISTRY_INITIAL_CAPACITY;
    registry->entries_count = 0;

    log_info("CAS registry created successfully with initial capacity: %d",
             CAS_REGISTRY_INITIAL_CAPACITY);
    return registry;
}

int cas_put(const cas_registry_t *registry, const char *key, void *value, size_t value_size,
            char *output_path) {
    if (!registry || !key || !value || !output_path) {
        log_error("Invalid parameters in cas_put");
        return -1;
    }

    log_debug("CAS PUT: storing key '%s', size: %zu bytes", key, value_size);

    if (cas_create_directory(registry, key, output_path) != 0) {
        log_error("Failed to create directory structure for key '%s'", key);
        return -1;
    }

    log_debug("CAS PUT: created directory structure at: %s", output_path);

    // ho il path, ci devo scrivere dentro il contenuto di value
    char complete_path[512];
    sprintf(complete_path, "%s/%s", output_path, "value.dat");
    FILE *fp = fopen(complete_path, "wb");
    if (!fp) {
        log_error("Failed to open file for writing: %s", complete_path);
        return -1;
    }
    if (fwrite(value, 1, value_size, fp) != value_size) {
        log_error("Failed to write complete data to file: %s", complete_path);
        fclose(fp);
        return -9;
    }
    fclose(fp);
    log_debug("CAS PUT: successfully wrote value data to: %s", complete_path);

    sprintf(complete_path, "%s/%s", output_path, "time.dat");
    fp = fopen(complete_path, "wb");
    if (!fp) {
        log_error("Failed to open timestamp file for writing: %s", complete_path);
        return -1;
    }
    time_t now = time(NULL);
    fprintf(fp, "%ld", (long)now);
    fclose(fp);
    log_debug("CAS PUT: successfully wrote timestamp to: %s", complete_path);

    log_info("CAS PUT: successfully stored key '%s' at: %s", key, output_path);
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
    if (!key || !registry) {
        log_error("Invalid parameters in cas_evict");
        return -1;
    }

    log_debug("CAS EVICT: attempting to remove key '%s'", key);

    char hash[65] = {'\0'};
    char path[512] = {'\0'};

    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);

    // Array dei path da rimuovere (dal più profondo al più superficiale)
    const char *patterns[] = {"%s/%s/%s/%s/%s/value.dat", "%s/%s/%s/%s/%s", "%s/%s/%s/%s",
                              "%s/%s/%s", "%s/%s"};

    int founded = 1;
    for (int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), patterns[i], registry->base_path, fs_path->p[0], fs_path->p[1],
                 fs_path->p[2], fs_path->p[3]);
        log_debug("CAS EVICT: removing path: %s", path);
        if (remove_dir(path) == -1) {
            log_debug("CAS EVICT: failed to remove path: %s", path);
            founded = 0;
        }
    }
    if (founded == 0) {
        log_warn("CAS EVICT: failed to remove some paths for key '%s'", key);
        free(fs_path);
        return -1;
    }

    char p[512] = {'\0'};
    sprintf(p, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1], fs_path->p[2],
            fs_path->p[3]);
    log_debug("CAS EVICT: removing entry from registry: %s", p);

    for (int i = 0; i < registry->entries_count; i++) {
        if (strcmp(registry->entries[i], p) == 0) {
            log_debug("CAS EVICT: found registry entry at index %d", i);
            for (int j = i; j < registry->entries_count - 1; j++) {
                registry->entries[j] = registry->entries[j + 1];
            }
            free(registry->entries[registry->entries_count - 1]);
            registry->entries_count--;
            log_debug("CAS EVICT: removed entry from registry, new count: %zu",
                      registry->entries_count);
            break;
        }
    }

    free(fs_path);
    log_info("CAS EVICT: successfully removed key '%s'", key);
    return 0;
}

int cas_add_to_registry(cas_registry_t *registry, char *path) {
    if (!registry || !path) {
        log_error("Invalid parameters in cas_add_to_registry");
        return -1;
    }

    log_debug("CAS REGISTRY: adding path to registry: %s", path);

    if (registry->entries_count + 1 >= registry->capacity) {
        size_t old_capacity = registry->capacity;
        registry->capacity *= 2;
        log_debug("CAS REGISTRY: expanding capacity from %zu to %zu", old_capacity,
                  registry->capacity);

        char **new_entries = realloc(registry->entries, registry->capacity * sizeof(char *));
        if (!new_entries) {
            log_error("Failed to reallocate registry entries");
            return -1; // Errore di allocazione
        }
        registry->entries = new_entries;
    }

    registry->entries[registry->entries_count] = malloc(strlen(path) + 1); // +1 per '\0'
    if (!registry->entries[registry->entries_count]) {
        log_error("Failed to allocate memory for registry entry");
        return -1; // Errore di allocazione
    }
    strcpy(registry->entries[registry->entries_count], path);
    registry->entries_count++;

    log_debug("CAS REGISTRY: successfully added entry, total count: %zu", registry->entries_count);
    return 0;
}

void cas_registry_destroy(cas_registry_t *registry) {
    if (!registry) {
        log_warn("Attempted to destroy NULL CAS registry");
        return;
    }

    log_info("Destroying CAS registry with %zu entries", registry->entries_count);

    if (registry->entries != NULL) {
        // Prima libera ogni singola stringa
        log_debug("CAS REGISTRY: cleaning up %zu entries", registry->entries_count);
        for (size_t i = 0; i < registry->entries_count; i++) {
            if (registry->entries[i] != NULL) {
                // TODO: rimuovi il file se esistente
                log_debug("CAS REGISTRY: freeing entry %zu: %s", i, registry->entries[i]);
                free(registry->entries[i]); // libera la stringa
            }
        }
        log_debug("CAS REGISTRY: cleaning up base path: %s", registry->base_path);
        cleanup(registry->base_path);
        // Poi libera l'array di puntatori
        free(registry->entries);
    }

    // Infine libera la struct
    free(registry);
    log_info("CAS registry destroyed successfully");
}

/* si usa con :
    void *data = NULL;           // Inizializza a NULL
    size_t size;
    int result = cas_get("my_key", &data, &size);
*/
int cas_get(const cas_registry_t *registry, const char *key, void **buffer, size_t *actual_size) {
    if (!registry || !key || !buffer || !actual_size) {
        log_error("Invalid parameters in cas_get");
        return -1;
    }

    log_debug("CAS GET: searching for key '%s'", key);

    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);
    char *path = get_path(registry, fs_path);
    char complete_path[PATH_MAX];
    sprintf(complete_path, "%s/%s", path, "value.dat");

    log_debug("CAS GET: looking for file at: %s", complete_path);

    struct stat st;
    if (stat(complete_path, &st) != 0) {
        log_debug("CAS GET: file not found for key '%s' at path: %s", key, complete_path);
        free_path(fs_path);
        free(path);
        return -1;
    }

    size_t file_size = st.st_size;
    log_debug("CAS GET: found file for key '%s', size: %zu bytes", key, file_size);

    *buffer = malloc(file_size);
    if (!*buffer) {
        log_error("Memory allocation failed for key '%s' (size: %zu)", key, file_size);
        free_path(fs_path);
        free(path);
        return -1; // Errore di allocazione
    }

    FILE *fp = fopen(complete_path, "rb");
    if (!fp) {
        log_error("Failed to open file for reading: %s", complete_path);
        free(*buffer);
        free_path(fs_path);
        free(path);
        return -1;
    }

    size_t read_size = fread(*buffer, 1, file_size, fp);
    if (read_size < file_size) {
        log_error("Failed to read complete file for key '%s' (read: %zu, expected: %zu)", key,
                  read_size, file_size);
        free(*buffer);
        free_path(fs_path);
        free(path);
        fclose(fp);
        return -1;
    }

    *actual_size = read_size;
    fclose(fp);
    free_path(fs_path);
    free(path);

    log_info("CAS GET: successfully retrieved key '%s', size: %zu bytes", key, read_size);
    return 0;
}

/* ========================================
 * static functions
 * ======================================== */

static int cas_create_directory(const cas_registry_t *registry, const char *key,
                                char *output_path) {
    if (!key) return -1;

    char hash[65] = {'\0'};
    sha256_string(key, hash);
    fs_path_t *fs_path = create_fs_path(hash);

    char path[512] = {'\0'};
    sprintf(path, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1],
            fs_path->p[2], fs_path->p[3]);
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

    sprintf(path, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1],
            fs_path->p[2], fs_path->p[3]);
    if (mkdir(path, 0755) != 0) return return_and_free(-1, fs_path);

    strcpy(output_path, path);
    return return_and_free(0, fs_path);
}

static int cas_remove(const cas_registry_t *registry, fs_path_t *fs_path) {
    char path[512];

    sprintf(path, "%s/%s/%s/%s/%s/value.dat", registry->base_path, fs_path->p[0], fs_path->p[1],
            fs_path->p[2], fs_path->p[3]);
    remove(path);

    sprintf(path, "%s/%s/%s/%s/%s", registry->base_path, fs_path->p[0], fs_path->p[1],
            fs_path->p[2], fs_path->p[3]);
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
    if (!r) return NULL;

    for (int i = 0; i < 4; i++) {
        r->p[i] = malloc(CHUNK_PATH + 1);
        if (!r->p[i]) {
            // Cleanup allocazioni precedenti in caso di errore
            for (int j = 0; j < i; j++) {
                free(r->p[j]);
            }
            free(r);
            return NULL;
        }
        substring(hash, i, r->p[i]);
    }
    return r;
}

static char *get_path(const cas_registry_t *registry, const fs_path_t *path) {
    char *r = calloc(1, 512);
    if (!r) return NULL;

    sprintf(r, "%s/%s/%s/%s/%s", registry->base_path, path->p[0], path->p[1], path->p[2],
            path->p[3]);
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
        srand(time(NULL) ^ getpid()); // più entropia
        seeded = 1;
    }

    sprintf(buffer, "%s%08x", root_fs, (unsigned int)rand());
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
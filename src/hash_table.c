/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */

#include "../include/hash_table.h"

#include <stdlib.h>
#include <string.h>

static uint32_t hash_index_key(const char* key);
static hash_table_entry_t *find_entry(hash_table_t *table, char *key);

hash_table_t *hash_table_create(void) {
    hash_table_t *table = calloc(1, sizeof(hash_table_t));
    if (table == NULL) return NULL;

    return table;
}

void hash_table_destroy(hash_table_t *table) {
    if (table == NULL) return;

    for (int i=0 ; i < HASH_TABLE_SIZE ; i++) {
        hash_table_entry_t *current = table->buckets[i];
        while (current != NULL) {
            hash_table_entry_t *next = current->next;
            free(current->key);
            free(current->value);
            free(current);
            current = next;
        }
    }
    free(table);
}

int hash_table_put(hash_table_t *table, char *key, void *value, size_t entry_size) {

    if (table == NULL) return HASH_TABLE_PUT_ERROR;
    if (find_entry(table, key) != NULL) return HASH_TABLE_PUT_DUPLICATE;

    hash_table_entry_t *entry = malloc(sizeof(hash_table_entry_t));
    entry->key = strdup(key);
    entry->value = malloc(entry_size);
    memcpy(entry->value, value, entry_size);

    uint32_t index = hash_index_key(key);

    entry->next = table->buckets[index];
    table->buckets[index] = entry;
    table->count++;
    return HASH_TABLE_PUT_SUCCESS;
}

static hash_table_entry_t *find_entry(hash_table_t *table, char *key) {
    if (table == NULL) return NULL;

    uint32_t key_index = hash_index_key(key);
    hash_table_entry_t *current = table->buckets[key_index];
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void* hash_table_get(hash_table_t *table, char *key) {
    if (table == NULL) return NULL;

    hash_table_entry_t *entry = find_entry(table, key);
    if (entry == NULL) return NULL;

    return entry->value;
}

int hash_table_remove(hash_table_t *table, char *key) {
    if (table == NULL) return -1;

    uint32_t key_index = hash_index_key(key);
    hash_table_entry_t *current = table->buckets[key_index];
    hash_table_entry_t *previous = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            // trovato, ora dobbiamo rimuovere current

            if (previous == NULL) table->buckets[key_index] = current->next;
            else                  previous->next = current->next;

            free(current->key);
            free(current->value);
            free(current);

            table->count--;
            return 0;
        }
        previous = current;
        current = current->next;
    }
    return -1;
}

static uint32_t hash_djb2(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static uint32_t hash_index_key(const char* key) {
    return hash_djb2(key) % HASH_TABLE_SIZE;
}
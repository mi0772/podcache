/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 */
 
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <stddef.h>

#define HASH_TABLE_SIZE 1024

// Return codes
#define HASH_TABLE_PUT_SUCCESS 0
#define HASH_TABLE_PUT_ERROR -1
#define HASH_TABLE_PUT_DUPLICATE -2

typedef struct hash_entry {
    char* key;
    void* value;
    struct hash_entry* next;
} hash_table_entry_t;

typedef struct hash_table {
    hash_table_entry_t* buckets[HASH_TABLE_SIZE];
    size_t count;
} hash_table_t;

// Function declarations
hash_table_t* hash_table_create(void);
void hash_table_destroy(hash_table_t* table);
int hash_table_put(hash_table_t* table, char* key, void* value, size_t entry_size);
void* hash_table_get(hash_table_t* table, char* key);
int hash_table_remove(hash_table_t* table, char* key);

#endif

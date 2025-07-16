/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 16/07/25
 * License: MIT
 * File: test_hash_table.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/hash_table.h"

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            printf("✗ %s (FAILED)\n", message); \
        } \
    } while(0)

// Test: hash_table_create
void test_hash_table_create() {
    printf("\n=== Testing hash_table_create ===\n");

    hash_table_t* table = hash_table_create();

    TEST_ASSERT(table != NULL, "hash_table_create returns non-NULL");
    TEST_ASSERT(table->count == 0, "initial count is 0");

    // Check that all buckets are initialized to NULL
    int all_null = 1;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (table->buckets[i] != NULL) {
            all_null = 0;
            break;
        }
    }
    TEST_ASSERT(all_null, "all buckets initialized to NULL");

    hash_table_destroy(table);
}

// Test: hash_table_put basic functionality
void test_hash_table_put_basic() {
    printf("\n=== Testing hash_table_put basic ===\n");

    hash_table_t* table = hash_table_create();

    // Test inserting a simple string
    char* test_value = "hello world";
    int result = hash_table_put(table, "key1", test_value, strlen(test_value) + 1);

    TEST_ASSERT(result == HASH_TABLE_PUT_SUCCESS, "put returns success");
    TEST_ASSERT(table->count == 1, "count increments after put");

    // Test inserting different types
    int int_value = 42;
    result = hash_table_put(table, "key2", &int_value, sizeof(int));
    TEST_ASSERT(result == HASH_TABLE_PUT_SUCCESS, "put integer returns success");
    TEST_ASSERT(table->count == 2, "count is 2 after second put");

    hash_table_destroy(table);
}

// Test: hash_table_put with NULL parameters
void test_hash_table_put_null() {
    printf("\n=== Testing hash_table_put with NULL ===\n");

    hash_table_t* table = hash_table_create();
    char* test_value = "test";

    // Test NULL table
    int result = hash_table_put(NULL, "key", test_value, strlen(test_value) + 1);
    TEST_ASSERT(result == HASH_TABLE_PUT_ERROR, "put with NULL table returns error");

    // Test NULL key
    result = hash_table_put(table, NULL, test_value, strlen(test_value) + 1);
    TEST_ASSERT(result == HASH_TABLE_PUT_ERROR, "put with NULL key returns error");

    // Test NULL value
    result = hash_table_put(table, "key", NULL, 10);
    TEST_ASSERT(result == HASH_TABLE_PUT_ERROR, "put with NULL value returns error");

    hash_table_destroy(table);
}

// Test: hash_table_put duplicate keys
void test_hash_table_put_duplicate() {
    printf("\n=== Testing hash_table_put duplicates ===\n");

    hash_table_t* table = hash_table_create();

    char* value1 = "first value";
    char* value2 = "second value";

    // Insert first value
    int result1 = hash_table_put(table, "duplicate_key", value1, strlen(value1) + 1);
    TEST_ASSERT(result1 == HASH_TABLE_PUT_SUCCESS, "first put succeeds");

    // Try to insert same key again
    int result2 = hash_table_put(table, "duplicate_key", value2, strlen(value2) + 1);
    TEST_ASSERT(result2 == HASH_TABLE_PUT_DUPLICATE, "second put returns duplicate");
    TEST_ASSERT(table->count == 1, "count remains 1 after duplicate");

    hash_table_destroy(table);
}

// Test: hash_table_get basic functionality
void test_hash_table_get_basic() {
    printf("\n=== Testing hash_table_get basic ===\n");

    hash_table_t* table = hash_table_create();

    // Insert some values
    char* string_value = "test string";
    int int_value = 12345;

    hash_table_put(table, "string_key", string_value, strlen(string_value) + 1);
    hash_table_put(table, "int_key", &int_value, sizeof(int));

    // Test getting string value
    char* retrieved_string = (char*)hash_table_get(table, "string_key");
    TEST_ASSERT(retrieved_string != NULL, "get returns non-NULL for existing key");
    TEST_ASSERT(strcmp(retrieved_string, string_value) == 0, "retrieved string matches original");

    // Test getting int value
    int* retrieved_int = (int*)hash_table_get(table, "int_key");
    TEST_ASSERT(retrieved_int != NULL, "get returns non-NULL for int key");
    TEST_ASSERT(*retrieved_int == int_value, "retrieved int matches original");

    // Test getting non-existent key
    void* non_existent = hash_table_get(table, "non_existent_key");
    TEST_ASSERT(non_existent == NULL, "get returns NULL for non-existent key");

    hash_table_destroy(table);
}

// Test: hash_table_get with NULL parameters
void test_hash_table_get_null() {
    printf("\n=== Testing hash_table_get with NULL ===\n");

    hash_table_t* table = hash_table_create();

    // Test NULL table
    void* result = hash_table_get(NULL, "key");
    TEST_ASSERT(result == NULL, "get with NULL table returns NULL");

    // Test NULL key
    result = hash_table_get(table, NULL);
    TEST_ASSERT(result == NULL, "get with NULL key returns NULL");

    hash_table_destroy(table);
}

// Test: hash_table_remove basic functionality
void test_hash_table_remove_basic() {
    printf("\n=== Testing hash_table_remove basic ===\n");

    hash_table_t* table = hash_table_create();

    // Insert some values
    char* value1 = "value1";
    char* value2 = "value2";
    char* value3 = "value3";

    hash_table_put(table, "key1", value1, strlen(value1) + 1);
    hash_table_put(table, "key2", value2, strlen(value2) + 1);
    hash_table_put(table, "key3", value3, strlen(value3) + 1);

    TEST_ASSERT(table->count == 3, "count is 3 after inserting 3 items");

    // Remove middle item
    int result = hash_table_remove(table, "key2");
    TEST_ASSERT(result == 0, "remove returns 0 for existing key");
    TEST_ASSERT(table->count == 2, "count decrements after remove");

    // Verify item is actually removed
    void* removed_value = hash_table_get(table, "key2");
    TEST_ASSERT(removed_value == NULL, "removed item no longer retrievable");

    // Verify other items still exist
    char* still_there1 = (char*)hash_table_get(table, "key1");
    char* still_there3 = (char*)hash_table_get(table, "key3");
    TEST_ASSERT(still_there1 != NULL && strcmp(still_there1, value1) == 0, "key1 still exists");
    TEST_ASSERT(still_there3 != NULL && strcmp(still_there3, value3) == 0, "key3 still exists");

    hash_table_destroy(table);
}

// Test: hash_table_remove non-existent key
void test_hash_table_remove_nonexistent() {
    printf("\n=== Testing hash_table_remove non-existent ===\n");

    hash_table_t* table = hash_table_create();

    // Try to remove from empty table
    int result = hash_table_remove(table, "nonexistent");
    TEST_ASSERT(result == -1, "remove from empty table returns -1");

    // Insert something and try to remove different key
    char* value = "test";
    hash_table_put(table, "existing_key", value, strlen(value) + 1);

    result = hash_table_remove(table, "different_key");
    TEST_ASSERT(result == -1, "remove non-existent key returns -1");
    TEST_ASSERT(table->count == 1, "count unchanged after failed remove");

    hash_table_destroy(table);
}

// Test: hash_table_remove with NULL parameters
void test_hash_table_remove_null() {
    printf("\n=== Testing hash_table_remove with NULL ===\n");

    hash_table_t* table = hash_table_create();

    // Test NULL table
    int result = hash_table_remove(NULL, "key");
    TEST_ASSERT(result == -1, "remove with NULL table returns -1");

    // Test NULL key
    result = hash_table_remove(table, NULL);
    TEST_ASSERT(result == -1, "remove with NULL key returns -1");

    hash_table_destroy(table);
}

// Test: hash collisions
void test_hash_collisions() {
    printf("\n=== Testing hash collisions ===\n");

    hash_table_t* table = hash_table_create();

    // Insert multiple items (some may collide)
    char values[10][20];
    char keys[10][20];

    for (int i = 0; i < 10; i++) {
        sprintf(keys[i], "key_%d", i);
        sprintf(values[i], "value_%d", i);
        hash_table_put(table, keys[i], values[i], strlen(values[i]) + 1);
    }

    TEST_ASSERT(table->count == 10, "all 10 items inserted");

    // Verify all items can be retrieved
    int all_found = 1;
    for (int i = 0; i < 10; i++) {
        char* retrieved = (char*)hash_table_get(table, keys[i]);
        if (retrieved == NULL || strcmp(retrieved, values[i]) != 0) {
            all_found = 0;
            break;
        }
    }
    TEST_ASSERT(all_found, "all items retrievable after collision handling");

    hash_table_destroy(table);
}

// Test: memory stress test
void test_memory_stress() {
    printf("\n=== Testing memory stress ===\n");

    hash_table_t* table = hash_table_create();

    // Insert many items
    const int num_items = 1000;
    char keys[num_items][20];
    char values[num_items][50];

    for (int i = 0; i < num_items; i++) {
        sprintf(keys[i], "stress_key_%d", i);
        sprintf(values[i], "stress_value_%d_with_longer_content", i);
        hash_table_put(table, keys[i], values[i], strlen(values[i]) + 1);
    }

    TEST_ASSERT(table->count == num_items, "all stress test items inserted");

    // Random access test
    int random_checks = 100;
    int all_correct = 1;
    for (int i = 0; i < random_checks; i++) {
        int idx = rand() % num_items;
        char* retrieved = (char*)hash_table_get(table, keys[idx]);
        if (retrieved == NULL || strcmp(retrieved, values[idx]) != 0) {
            all_correct = 0;
            break;
        }
    }
    TEST_ASSERT(all_correct, "random access test passed");

    hash_table_destroy(table);
}

// Test: hash_table_destroy
void test_hash_table_destroy() {
    printf("\n=== Testing hash_table_destroy ===\n");

    // Test destroying NULL table
    hash_table_destroy(NULL);
    TEST_ASSERT(1, "destroying NULL table doesn't crash");

    // Test destroying empty table
    hash_table_t* empty_table = hash_table_create();
    hash_table_destroy(empty_table);
    TEST_ASSERT(1, "destroying empty table doesn't crash");

    // Test destroying table with items
    hash_table_t* table = hash_table_create();
    for (int i = 0; i < 10; i++) {
        char key[20], value[20];
        sprintf(key, "key_%d", i);
        sprintf(value, "value_%d", i);
        hash_table_put(table, key, value, strlen(value) + 1);
    }
    hash_table_destroy(table);
    TEST_ASSERT(1, "destroying table with items doesn't crash");
}

// Test: edge cases
void test_edge_cases() {
    printf("\n=== Testing edge cases ===\n");

    hash_table_t* table = hash_table_create();

    // Test empty string key
    char* empty_value = "empty_key_value";
    int result = hash_table_put(table, "", empty_value, strlen(empty_value) + 1);
    TEST_ASSERT(result == HASH_TABLE_PUT_SUCCESS, "empty string key works");

    char* retrieved = (char*)hash_table_get(table, "");
    TEST_ASSERT(retrieved != NULL && strcmp(retrieved, empty_value) == 0, "empty key retrievable");

    // Test very long key
    char long_key[1000];
    memset(long_key, 'a', 999);
    long_key[999] = '\0';
    char* long_value = "long_key_value";

    result = hash_table_put(table, long_key, long_value, strlen(long_value) + 1);
    TEST_ASSERT(result == HASH_TABLE_PUT_SUCCESS, "very long key works");

    retrieved = (char*)hash_table_get(table, long_key);
    TEST_ASSERT(retrieved != NULL && strcmp(retrieved, long_value) == 0, "long key retrievable");

    // Test zero-size value
    result = hash_table_put(table, "zero_size", "", 0);
    TEST_ASSERT(result == HASH_TABLE_PUT_SUCCESS, "zero-size value works");

    hash_table_destroy(table);
}

void run_all_tests() {
    printf("🧪 Running Hash Table Tests\n");
    printf("============================\n");

    test_hash_table_create();
    test_hash_table_put_basic();
    test_hash_table_put_null();
    test_hash_table_put_duplicate();
    test_hash_table_get_basic();
    test_hash_table_get_null();
    test_hash_table_remove_basic();
    test_hash_table_remove_nonexistent();
    test_hash_table_remove_null();
    test_hash_collisions();
    test_memory_stress();
    test_hash_table_destroy();
    test_edge_cases();

    printf("\n============================\n");
    printf("📊 Test Results: %d/%d passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("🎉 All tests passed! ✅\n");
    } else {
        printf("❌ %d tests failed!\n", tests_run - tests_passed);
    }
}

int main() {
    run_all_tests();
    return (tests_passed == tests_run) ? 0 : 1;
}

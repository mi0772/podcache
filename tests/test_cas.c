/**
 * Project: PodCache
 * Author: Carlo Di Giuseppe
 * Date: 17/07/25
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../include/cas.h"
#include "../include/hash_func.h"

// Struttura di test
typedef struct {
    int id;
    char name[50];
    float price;
} Product;

// Utility per stampare risultati
void print_test_result(const char* test_name, int result) {
    printf("%-30s: %s\n", test_name, result == 0 ? "PASS" : "FAIL");
}

void print_separator(const char* title) {
    printf("\n=== %s ===\n", title);
}

void test_strings() {
    print_separator("TEST STRINGHE");

    char *key1 = "string_test";
    char *value1 = "Questa e una stringa di test!";

    // Put stringa
    int result1 = cas_put(key1, value1, strlen(value1) + 1);
    print_test_result("String PUT", result1);

    // Get stringa
    void *buf1 = NULL;
    size_t buf1_size = 0;
    int get_result1 = cas_get(key1, &buf1, &buf1_size);
    print_test_result("String GET", get_result1);

    if (get_result1 == 0) {
        printf("Stringa recuperata: \"%s\"\n", (char*)buf1);
        printf("Dimensione: %zu bytes\n", buf1_size);
        assert(strcmp((char*)buf1, value1) == 0);
        free(buf1);
    }
}

void test_integers() {
    print_separator("TEST INTERI");

    int numbers[] = {42, -1337, 0, 2147483647, -2147483648};
    int num_count = sizeof(numbers) / sizeof(numbers[0]);

    for (int i = 0; i < num_count; i++) {
        char key[50];
        sprintf(key, "int_test_%d", i);

        // Put intero
        int put_result = cas_put(key, &numbers[i], sizeof(int));
        printf("PUT int %d: %s\n", numbers[i], put_result == 0 ? "PASS" : "FAIL");

        // Get intero
        void *data = NULL;
        size_t size;
        int get_result = cas_get(key, &data, &size);

        if (get_result == 0) {
            int recovered = *(int*)data;
            printf("GET int %d: %s (valore: %d)\n",
                   numbers[i],
                   recovered == numbers[i] ? "PASS" : "FAIL",
                   recovered);
            assert(recovered == numbers[i]);
            free(data);
        } else {
            printf("GET int %d: FAIL - ERRORE\n", numbers[i]);
        }
    }
}

void test_floats() {
    print_separator("TEST FLOAT");

    float pi = 3.14159f;
    int float_put = cas_put("float_test", &pi, sizeof(float));
    print_test_result("Float PUT", float_put);

    void *float_data = NULL;
    size_t float_size;
    int float_get = cas_get("float_test", &float_data, &float_size);

    if (float_get == 0) {
        float recovered_pi = *(float*)float_data;
        printf("Pi recuperato: %.5f\n", recovered_pi);
        assert(recovered_pi == pi);
        free(float_data);
    }
}

void test_structs() {
    print_separator("TEST STRUCT");

    Product products[] = {
        {1, "iPhone 15", 999.99f},
        {2, "MacBook Pro", 2499.99f},
        {3, "AirPods", 179.99f}
    };

    for (int i = 0; i < 3; i++) {
        char key[50];
        sprintf(key, "product_%d", products[i].id);

        // Put struct
        int struct_put = cas_put(key, &products[i], sizeof(Product));
        printf("PUT Product %d: %s\n", products[i].id, struct_put == 0 ? "PASS" : "FAIL");

        // Get struct
        void *product_data = NULL;
        size_t product_size;
        int struct_get = cas_get(key, &product_data, &product_size);

        if (struct_get == 0) {
            Product *recovered = (Product*)product_data;
            printf("GET Product %d: PASS (ID: %d, Nome: %s, Prezzo: %.2f)\n",
                   products[i].id, recovered->id, recovered->name, recovered->price);
            assert(recovered->id == products[i].id);
            assert(strcmp(recovered->name, products[i].name) == 0);
            assert(recovered->price == products[i].price);
            free(product_data);
        } else {
            printf("GET Product %d: FAIL - ERRORE\n", products[i].id);
        }
    }
}

void test_arrays() {
    print_separator("TEST ARRAY");

    int scores[] = {95, 87, 92, 78, 99, 85, 90};
    int scores_count = sizeof(scores) / sizeof(scores[0]);

    // Put array
    int array_put = cas_put("scores_array", scores, sizeof(scores));
    print_test_result("Array PUT", array_put);

    // Get array
    void *array_data = NULL;
    size_t array_size;
    int array_get = cas_get("scores_array", &array_data, &array_size);

    if (array_get == 0) {
        int *recovered_scores = (int*)array_data;
        int recovered_count = array_size / sizeof(int);

        printf("Array recuperato (%d elementi): ", recovered_count);
        for (int i = 0; i < recovered_count; i++) {
            printf("%d ", recovered_scores[i]);
        }
        printf("\n");

        assert(recovered_count == scores_count);
        for (int i = 0; i < recovered_count; i++) {
            assert(recovered_scores[i] == scores[i]);
        }
        free(array_data);
    }
}

void test_binary_data() {
    print_separator("TEST DATI BINARI");

    unsigned char binary_data[] = {0x00, 0xFF, 0x42, 0xAB, 0xCD, 0xEF};
    int binary_put = cas_put("binary_test", binary_data, sizeof(binary_data));
    print_test_result("Binary PUT", binary_put);

    void *binary_retrieved = NULL;
    size_t binary_size;
    int binary_get = cas_get("binary_test", &binary_retrieved, &binary_size);

    if (binary_get == 0) {
        unsigned char *bytes = (unsigned char*)binary_retrieved;
        printf("Dati binari recuperati: ");
        for (size_t i = 0; i < binary_size; i++) {
            printf("0x%02X ", bytes[i]);
        }
        printf("\n");

        assert(binary_size == sizeof(binary_data));
        for (size_t i = 0; i < binary_size; i++) {
            assert(bytes[i] == binary_data[i]);
        }
        free(binary_retrieved);
    }
}

void test_hash_collisions() {
    print_separator("TEST HASH COLLISION");

    // Test con chiavi diverse ma simili
    char *similar_keys[] = {"test", "test1", "test2", "1test", "TEST"};
    int similar_count = sizeof(similar_keys) / sizeof(similar_keys[0]);

    for (int i = 0; i < similar_count; i++) {
        char hash[65];
        sha256_string(similar_keys[i], hash);
        printf("Key: %-6s -> Hash: %.16s...\n", similar_keys[i], hash);

        // Salva un valore diverso per ogni chiave
        int unique_value = i * 100;
        cas_put(similar_keys[i], &unique_value, sizeof(int));
    }

    // Verifica che i valori siano diversi
    printf("\nVerifica valori univoci:\n");
    for (int i = 0; i < similar_count; i++) {
        void *data = NULL;
        size_t size;
        if (cas_get(similar_keys[i], &data, &size) == 0) {
            int value = *(int*)data;
            printf("Key: %-6s -> Valore: %d %s\n",
                   similar_keys[i], value,
                   value == i * 100 ? "PASS" : "FAIL");
            assert(value == i * 100);
            free(data);
        }
    }
}

void test_error_handling() {
    print_separator("TEST GESTIONE ERRORI");

    // Test chiave NULL
    int null_key_result = cas_put(NULL, "test", 4);
    printf("PUT con chiave NULL: %s\n", null_key_result != 0 ? "PASS - ERRORE GESTITO" : "FAIL - DOVREBBE FALLIRE");

    // Test get di chiave inesistente
    void *missing_data = NULL;
    size_t missing_size;
    int missing_result = cas_get("chiave_inesistente_xyz", &missing_data, &missing_size);
    printf("GET chiave inesistente: %s\n", missing_result != 0 ? "PASS - ERRORE GESTITO" : "FAIL - DOVREBBE FALLIRE");

    // Test buffer NULL per get
    int null_buffer_result = cas_get("test", NULL, &missing_size);
    printf("GET con buffer NULL: %s\n", null_buffer_result != 0 ? "PASS - ERRORE GESTITO" : "FAIL - DOVREBBE FALLIRE");
}

int main(void) {
    printf("CAS System Test Suite\n");
    printf("=====================\n");

    // Esegui tutti i test
    test_strings();
    test_integers();
    test_floats();
    test_structs();
    test_arrays();
    test_binary_data();
    test_hash_collisions();
    test_error_handling();

    // Riepilogo
    print_separator("RIEPILOGO");
    printf("Test completati!\n");
    printf("- Stringhe, interi, float, struct, array, dati binari\n");
    printf("- Hash collision check\n");
    printf("- Gestione errori\n");
    printf("\nCAS System e operativo!\n");

    return 0;
}
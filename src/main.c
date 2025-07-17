#include <stdio.h>

#include "cas.h"
#include "hash_func.h"


int main(void) {
    char *key = "prova";
    char hash[65];
    sha256_string(key, hash);

    cas_create_result_e r = cas_create_directory(key);
    printf("%d\n", r);

}

#include <stdlib.h>
#include <string.h>

#include "../include/clogger.h"
#include "../include/pod_cache.h"
#include "../include/server_tcp.h"

int main(void) {
    clog_init(LOG_LEVEL_INFO, "podcache.log");
    log_info("PodCache server starting up...");

    log_debug("Initializing TCP server");
    tcp_server_start();

    log_info("PodCache server shutdown complete");
    return 0;
}

int test_cache_main(void) {
    clog_init(LOG_LEVEL_DEBUG, "clogger.log");

    log_info("PodCache - v-alpha-0.0.1");

    pod_cache_t *pod_cache_g = pod_cache_create(MB_TO_BYTES(10), 1);
    log_info("main cache holder created");

    log_info("put 1 test element");
    pod_cache_put(pod_cache_g, "carlo", "stringa di prova", strlen("stringa di prova"));

    log_info("get 1 test element");
    void *value = NULL;
    size_t value_size = 0;
    if (pod_cache_get(pod_cache_g, "carlo", &value, &value_size) == 0) {
        log_info("carlo found");
        log_info("carlo value is %s", (char *)value);
        free(value);
    }
    pod_cache_destroy(pod_cache_g);

    log_info("inizializzo nuova cache con dimensione molto piccola");

    pod_cache_t *pod_cache = pod_cache_create(1024, 2); // solo 1 bytes

    char key[65];
    char v[512];
    int c = 0;
    int latest_partition = 0;
    do {
        c++;

        sprintf(key, "test_%d", c);
        sprintf(v, "value of %d", c);
        latest_partition = pod_cache_put(pod_cache, key, v, strlen(v));
        log_info("bytes occupati su partizione %d : %d su %d", latest_partition,
                 pod_cache->partitions[latest_partition]->current_bytes_size,
                 pod_cache->partitions[latest_partition]->max_bytes_capacity);
    } while (pod_cache->partitions[latest_partition]->max_bytes_capacity >
             (pod_cache->partitions[latest_partition]->current_bytes_size + strlen(v)));
    log_info("memoria terminata sulla partizione %d provo a scrivere ulteriore record",
             latest_partition);
    pod_cache_put(pod_cache, "test_finale", "test_finale", strlen("test_finale"));

    log_info("get test_1 che dovrebbe essere ormai su disco");

    if (pod_cache_get(pod_cache, "test_1", &value, &value_size) == 0) {
        log_info("carlo found");
        log_info("carlo value is %s", (char *)value);
        free(value);
    }

    log_info("get test_1 ancora che dovrebbe essere in memoria ora");

    if (pod_cache_get(pod_cache, "test_1", &value, &value_size) == 0) {
        log_info("carlo found");
        log_info("carlo value is %s", (char *)value);
        free(value);
    }
    pod_cache_destroy(pod_cache);

    return 0;
}

#include <stdlib.h>
#include <string.h>


#include "clogger.h"
#include "pod_cache.h"


int main(void) {
    clog_init(LOG_LEVEL_DEBUG, "clogger.log");

    log_info("PodCache - v-alpha-0.0.1");

    pod_cache_t *pod_cache = pod_cache_create(MB_TO_BYTES(10), true);
    log_info("main cache holder created");

    log_info("put 1 test element");
    pod_cache_put(pod_cache, "carlo", "stringa di prova", strlen("stringa di prova"));

    log_info("get 1 test element");
    void *value = NULL;
    size_t value_size = 0;
    if (pod_cache_get(pod_cache, "carlo", &value, &value_size) == 0) {
        log_info("carlo found");
        log_info("carlo value is %s", (char*)value);
        free(value);
    }

    return 0;
}
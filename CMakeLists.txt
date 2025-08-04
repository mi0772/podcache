cmake_minimum_required(VERSION 3.16)
project(PodCache C)

set(CMAKE_C_STANDARD 99)

# Configurazione specifica per piattaforma
# Using standalone SHA-256 implementation (no OpenSSL dependency)
message(STATUS "Using standalone SHA-256 implementation (no OpenSSL)")

# Abilita i test
enable_testing()

# La tua libreria principale
add_library(podcache_lib
        src/toml.c
        include/toml.h
        src/pod_cache.c
        src/lru_cache.c
        src/hash_func_nossl.c
        include/hash_func.h
        src/cas.c
        include/cas.h
        src/clogger.c
        include/clogger.h
        src/resp_parser.c
        include/resp_parser.h
)

target_include_directories(podcache_lib PUBLIC include)

# Eseguibile principale
add_executable(podcache src/main.c
        src/toml.c
        include/toml.h
        src/clogger.c
        include/clogger.h

        src/lru_cache.c
        include/lru_cache.h
        src/hash_func_nossl.c
        include/hash_func.h
        src/cas.c
        include/cas.h
        src/server_tcp.c
        include/server_tcp.h
        src/resp_parser.c
        include/resp_parser.h
)
target_link_libraries(podcache podcache_lib)

# Subdirectory per i test
add_subdirectory(tests)
cmake_minimum_required(VERSION 3.16)
project(PodCache C)

set(CMAKE_C_STANDARD 99)

# Abilita i test
enable_testing()

# La tua libreria principale
add_library(podcache_lib
        src/cache.c
        src/hash_table.c
        src/lru_cache.c
        src/hash_func.c
        include/hash_func.h
)

target_include_directories(podcache_lib PUBLIC include)

# Eseguibile principale
add_executable(podcache src/main.c
        src/lru_cache.c
        include/lru_cache.h
        src/hash_func.c
        include/hash_func.h)
target_link_libraries(podcache podcache_lib)

# Subdirectory per i test
add_subdirectory(tests)
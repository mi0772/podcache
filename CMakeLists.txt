cmake_minimum_required(VERSION 3.16)
project(PodCache C)

set(CMAKE_C_STANDARD 99)

# Configurazione specifica per piattaforma
if(APPLE)
    set(OPENSSL_ROOT_DIR
            /opt/homebrew/opt/openssl@3
            /opt/homebrew/opt/openssl
            /usr/local/opt/openssl@3
            /usr/local/opt/openssl
    )
elseif(UNIX AND NOT APPLE)

endif()

find_package(OpenSSL REQUIRED)

if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
else()
    message(FATAL_ERROR "OpenSSL not found!")
endif()

# Abilita i test
enable_testing()

# La tua libreria principale
add_library(podcache_lib
        src/toml.c
        include/toml.h
        src/pod_cache.c
        src/lru_cache.c
        src/hash_func.c
        include/hash_func.h
        src/cas.c
        include/cas.h
        src/clogger.c
        include/clogger.h
        src/resp_parser.c
        include/resp_parser.h
)

target_include_directories(podcache_lib PUBLIC include)
target_link_libraries(podcache_lib OpenSSL::SSL OpenSSL::Crypto)

# Eseguibile principale
add_executable(podcache src/main.c
        src/toml.c
        include/toml.h
        src/clogger.c
        include/clogger.h

        src/lru_cache.c
        include/lru_cache.h
        src/hash_func.c
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
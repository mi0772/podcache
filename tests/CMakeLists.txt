# Test per CAS system
add_executable(test_cas
        test_resp.c
        ../src/b64.c
        ../include/b64.h
        ../src/resp_parser.c
        ../include/resp_parser.h
)
target_link_libraries(test_cas podcache_lib)
add_test(NAME cas_tests COMMAND test_cas)


# Configura le directory per i test
set_tests_properties(cas_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Se vuoi eseguire tutti i test con 'make test'
# cmake --build . --target test
# Test per CAS system
add_executable(test_cas test_cas.c)
target_link_libraries(test_cas podcache_lib)
add_test(NAME cas_tests COMMAND test_cas)

# Test per hash table (se esiste)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_hash_table.c")
    add_executable(test_hash_table test_hash_table.c)
    target_link_libraries(test_hash_table podcache_lib)
    add_test(NAME hash_table_tests COMMAND test_hash_table)
endif()

# Configura le directory per i test
set_tests_properties(cas_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Se vuoi eseguire tutti i test con 'make test'
# cmake --build . --target test
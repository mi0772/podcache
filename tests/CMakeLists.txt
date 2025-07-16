# Test per hash table
add_executable(test_hash_table test_hash_table.c
)
target_link_libraries(test_hash_table podcache_lib)
add_test(NAME hash_table_tests COMMAND test_hash_table)
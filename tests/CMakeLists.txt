add_executable(my_lib_tests
  test_queue.cpp
)

target_link_libraries(my_lib_tests PRIVATE lockfree_ring_queue::lockfree_ring_queue)

add_test(NAME my_lib_tests COMMAND my_lib_tests)

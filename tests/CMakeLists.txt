set(TESTS)

list(APPEND TESTS
    test_queue
    test_queue_spsc
    test_queue_mpsc
    test_queue_mpmc
)

foreach(test_name IN LISTS TESTS)
    
    add_executable(
        ${test_name}
        ${test_name}.cpp
    )

    target_link_libraries(
        ${test_name} PRIVATE 
        lockfree_ring_queue::lockfree_ring_queue)

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )

endforeach()

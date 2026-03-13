#include <array>
#include <atomic>
#include <cassert>
#include <barrier>
#include <memory>
#include <thread>
#include <vector>

#include "lockfree_ring_queue.h"


constexpr int items_per_producer = 10000;


struct test_config
{
    int producers;
    int consumers;
};


void producer(atomic_ring::queue<int>& q,
              int producer_id,
              int items_per_producer,
              std::barrier<>& start_barrier)
{
    start_barrier.arrive_and_wait();

    const int start = producer_id * items_per_producer;

    for (int i = 0; i < items_per_producer; ++i)
    {
        const int value = start + i;

        while (!q.try_push(value))
            std::this_thread::yield();
    }
}


void consumer(atomic_ring::queue<int>& q,
              std::atomic<int>* seen,
              std::atomic<int>& consumed_count,
              int total_items,
              std::barrier<>& start_barrier)
{
    start_barrier.arrive_and_wait();

    int value;

    while (consumed_count.load(std::memory_order_acquire) < total_items)
        if (q.try_pop(value))
        {
            seen[value].fetch_add(1, std::memory_order_relaxed);
            consumed_count.fetch_add(1, std::memory_order_release);
        }
        else
        {
            std::this_thread::yield();
        }
}


void run_mpmc_test(int capacity, int producer_count, int consumer_count)
{
    const int total_items = producer_count * items_per_producer;

    atomic_ring::queue<int> q(capacity);

    std::unique_ptr<std::atomic<int>[]> seen(new std::atomic<int>[total_items]);
    for (int i = 0; i < total_items; ++i)
        seen[i].store(0, std::memory_order_relaxed);

    std::atomic<int> consumed_count = 0;

    std::barrier start_barrier(producer_count + consumer_count);

    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;

    producer_threads.reserve(producer_count);
    consumer_threads.reserve(consumer_count);

    for (int i = 0; i < producer_count; ++i)
        producer_threads.emplace_back(producer,
                                      std::ref(q),
                                      i,
                                      items_per_producer,
                                      std::ref(start_barrier));

    for (int i = 0; i < consumer_count; ++i)
        consumer_threads.emplace_back(consumer,
                                      std::ref(q),
                                      seen.get(),
                                      std::ref(consumed_count),
                                      total_items,
                                      std::ref(start_barrier));

    for (auto& t : producer_threads)
        t.join();

    for (auto& t : consumer_threads)
        t.join();

    assert(consumed_count.load(std::memory_order_acquire) == total_items);

    for (int i = 0; i < total_items; ++i)
        assert(seen[i].load(std::memory_order_relaxed) == 1);
}


int main()
{
    constexpr std::array<int, 12> capacities = {
        2, 4, 8, 16,
        64, 256,
        1024, 4096,
        16384, 65536,
        262144, 1048576
    };

    constexpr std::array<test_config, 10> configs = {{
        {1, 1},
        {2, 1},
        {1, 2},
        {2, 2},
        {4, 2},
        {2, 4},
        {4, 4},
        {8, 4},
        {4, 8},
        {8, 8}
    }};

    for (const auto& config : configs)
        for (int capacity : capacities)
            for (int repeat = 0; repeat < 20; ++repeat)
                run_mpmc_test(capacity, config.producers, config.consumers);

    return 0;
}

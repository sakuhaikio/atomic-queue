#include <cassert>
#include <thread>
#include <vector>
#include <barrier>

#include "lockfree_ring_queue.h"


constexpr int producers = 4;
constexpr int items_per_producer = 10000;
constexpr int total_items = producers * items_per_producer;


void producer(atomic_ring::queue<int>& q, int id, std::barrier<>& start_barrier)
{
    start_barrier.arrive_and_wait();

    int start = id * items_per_producer;

    for (int i = 0; i < items_per_producer; ++i)
    {
        int value = start + i;

        while (!q.try_push(value))
            std::this_thread::yield();
    }
}


void consumer(atomic_ring::queue<int>& q,
              std::vector<int>& seen,
              std::barrier<>& start_barrier)
{
    start_barrier.arrive_and_wait();

    int popped = 0;
    int value;

    while (popped < total_items)
        if (q.try_pop(value))
        {
            seen[value]++;
            popped++;
        }
        else
            std::this_thread::yield();
}


void run_mpsc_test(int capacity)
{
    atomic_ring::queue<int> q(capacity);

    std::vector<int> seen(total_items, 0);

    std::barrier start_barrier(producers + 1);

    std::vector<std::thread> threads;

    for (int i = 0; i < producers; ++i)
    {
        threads.emplace_back(producer,
                             std::ref(q),
                             i,
                             std::ref(start_barrier));
    }

    std::thread consumer_thread(
        consumer,
        std::ref(q),
        std::ref(seen),
        std::ref(start_barrier)
    );

    for (auto& t : threads)
        t.join();

    consumer_thread.join();

    for (int i = 0; i < total_items; ++i)
    {
        assert(seen[i] == 1);
    }
}

int main()
{
constexpr std::array<int, 12> capacities = {
    2, 4, 8, 16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576
};

    for (int capacity : capacities)
        for (int repeat = 0; repeat < 20; ++repeat)
            run_mpsc_test(capacity);

    return 0;
}

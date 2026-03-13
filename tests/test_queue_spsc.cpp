#include <cassert>
#include <thread>
#include <vector>
#include <barrier>

#include "lockfree_ring_queue.h"


constexpr int capacity = 4;
constexpr int N = 100000;


void producer(atomic_ring::queue<int>& q, std::barrier<>& start_barrier)
{
    start_barrier.arrive_and_wait();

    for (int i = 0; i < N; ++i)
    {
        while (!q.try_push(i))
            std::this_thread::yield();
    }
}


void consumer(atomic_ring::queue<int>& q,
              std::vector<int>& result,
              std::barrier<>& start_barrier)
{
    start_barrier.arrive_and_wait();

    int value;

    for (int i = 0; i < N; ++i)
    {
        while (!q.try_pop(value))
            std::this_thread::yield();

        result.push_back(value);
    }
}

void run_spsc_test(int capacity)
{
    atomic_ring::queue<int> q(capacity);

    std::vector<int> result;
    result.reserve(N);

    std::barrier start_barrier(2);

    std::thread producer_thread(producer, std::ref(q), std::ref(start_barrier));
    std::thread consumer_thread(consumer, std::ref(q), std::ref(result), std::ref(start_barrier));

    producer_thread.join();
    consumer_thread.join();

    assert(result.size() == N);

    for (int i = 0; i < N; ++i)
    {
        assert(result[i] == i);
    }
}

int main()
{
constexpr std::array<int, 12> capacities = {
    2, 4, 8, 16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576
};

    for (int capacity : capacities)
        for (int repeat = 0; repeat < 20; ++repeat)
            run_spsc_test(capacity);

    return 0;
}

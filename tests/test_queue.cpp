#include <iostream>
#include "lockfree_ring_queue.h"
#include <thread>
#include <vector>
#include <barrier>
#include <atomic>
#include <random>
#include <concepts>
#include <math.h>
#include <chrono>

const int THREAD_COUNT = (std::thread::hardware_concurrency() ? (int)std::thread::hardware_concurrency() : 8);


class rng
{
public:
    using engine_t = std::mt19937_64;

    rng() : engine(seed()){};
    explicit rng(uint64_t seed) : engine(seed) {}

    template<std::integral T>
    T uniform(T a, T b) { return std::uniform_int_distribution<>(a,b)(engine); }

private:
    engine_t engine;

    static uint64_t seed() {
        std::random_device rd;
        uint64_t s = 0;
        for (int i = 0; i < 4; ++i)
            s = (s << 16) ^ rd();
        return s;
    }
};

int main()
{

    for(int d = 0; d < 10000; ++d)
    {

        rng r;
        int total_theads = r.uniform(2, THREAD_COUNT);
        int in_out_split = r.uniform(1, total_theads-1);
        int queue_size = (1 << r.uniform(2, 16)) - 1;
        

        std::vector<std::thread> in_pool;
        std::vector<std::thread> out_pool;
        std::barrier start_line(total_theads);


        atomic_ring::queue<int> queue(1024);
        
        std::atomic<int> counter = 0;
        std::atomic<int> pushing = in_out_split + 1;

        auto start = std::chrono::high_resolution_clock::now();
        for(int id = 0; id < in_out_split; ++id)
        {
            in_pool.emplace_back([&](){
                start_line.arrive_and_wait();
                for(int i = 0; i < 100000; ++i)
                    if(queue.try_push(1))
                        counter.fetch_add(1, std::memory_order_relaxed);        
            });

            pushing.fetch_add(-1); 
        }

        for(int id = 0; id < total_theads - in_out_split; ++id)
        {
            out_pool.emplace_back([&](){
                start_line.arrive_and_wait();
                int result;
                int retry_counter = 0;

                while(pushing.load(std::memory_order_relaxed) > 0 || counter.load(std::memory_order_relaxed) > 0)
                {
                    if(queue.try_pop(result))
                    {
                        counter.fetch_add(-result, std::memory_order_relaxed);        
                    }
                    else
                    {
                        if (++retry_counter >= 5)
                        {
                            std::this_thread::yield();
                            retry_counter = 0;
                        }
                    }
                }
            });
        }

        for (auto& t : in_pool) t.join();
        pushing.fetch_add(-1); 
        for (auto& t : out_pool) t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "TEST NUMBER: " << d << std::endl;
        std::cout << "  pushing threads: " << in_out_split << std::endl;
        std::cout << "  popping threads: " << total_theads - in_out_split << std::endl;
        std::cout << "  queue_size : " << queue_size << std::endl;
        std::cout << "  time : " << duration << std::endl;
        if(counter != 0)
        {
            return 1;
        }
    }



}

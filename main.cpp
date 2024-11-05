#include <iostream>
#include <atomic>
#include <concepts>
#include <utility>
#include <math.h>
#include <syncstream>
#include <exception>

#include <vector>
#include <numeric>
#include <thread>
#include <mutex>
#include <chrono>

#include "atomic_queue.hh"

#define DEBUGPRINT
#undef DEBUGPRINT

/* x.compare_exchange_strong(y,z):
     Atomically:
       if x == y
           x = z
           return true
       else 
           y = x
           return false
*/

class tester
{
public:
    tester(int queue_size, int th_in, int th_out, int count);
    void spam_values_in();
    void spam_values_out();
    void run();

    atomic_queue<int> queue;

    std::mutex mt;
    std::mutex mt_push;

    int in_threads_count;
    int out_threads_count;
    
    int count;
};

tester::tester(int queue_size, int th_in, int th_out, int test_count) :
    queue(atomic_queue<int>(1 << queue_size)),
    in_threads_count(th_in),
    out_threads_count(th_out),
    count(test_count)
{
}

void tester::spam_values_in()
{
    std::cout << "thread´" << std::endl;
    auto start = std::chrono::steady_clock::now();

    while(true)
    {
        queue.try_push(0);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if(elapsed >= 1)
            break;
    }
}

void tester::spam_values_out()
{
    auto start = std::chrono::steady_clock::now();

    while(true)
    {
        int que_value;
        queue.try_pop(que_value);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if(elapsed >= 1)
            break;
    }
}

void tester::run()
{
    std::vector<std::thread> threads_in;

    std::vector<std::thread> threads_out;

    for(int i = 0; i < in_threads_count; i++)
        threads_in.emplace_back(std::move(std::thread(&tester::spam_values_in, this ))); 

    for(int i = 0; i < out_threads_count; i++)
        threads_out.emplace_back(std::move(std::thread(&tester::spam_values_out, this))); 

    for(auto &a : threads_in)
        a.join();

    for(auto &a : threads_out)
        a.join();
}


void test()
{
    int queue_size = (rand() % 15) + 1;
    int th_in = (rand() % 6) + 1;
    int th_out = (rand() % (7 - th_in)) + 1;
    int count = (rand() % (100000)) + 1;

    th_in = 1;
    th_out = 5;

    tester test(20, th_in, th_out, count);
    test.run();


//    std::cout << test.queue.items_in << std::endl;
//    std::cout << test.queue.items_out << std::endl;
//    std::cout << test.queue.items_in - test.queue.items_out << std::endl;
}


int main()
{
    test();
}



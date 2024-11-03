#include <iostream>
#include <atomic>
#include <concepts>
#include <utility>
#include <math.h>
#include <syncstream>

#include <vector>
#include <numeric>
#include <thread>
#include "atomic_queue.hh"


class tester
{
public:
    tester(int queue_size, int th_in, int th_out, int count);
    void spam_values_in(int &vlaue);
    void spam_values_out(int &value);
    int run();

private:
    atomic_queue<int> queue;

    int in_threads_count;
    int out_threads_count;
    
    std::atomic_uint pushed_items; 

    int count;
};

tester::tester(int queue_size, int th_in, int th_out, int test_count) :
    queue(atomic_queue<int>(1 << queue_size)),
    in_threads_count(th_in),
    out_threads_count(th_out),
    pushed_items(th_in),
    count(test_count)
{
}

void tester::spam_values_in(int &value)
{
    std::vector<int> v;
    for(int i = 0; i < count; i++)
        if(queue.try_push(1))
        {
            v.push_back(1);
            pushed_items++;
        }

    pushed_items--;

    value = std::accumulate(v.begin(), v.end(), 0);
}

void tester::spam_values_out(int &value)
{
    std::vector<int> v;
    while(pushed_items > 0)
    {
        //std::cout << "pushed_items: " << pushed_items << std::endl;
        int que_value;
        if(queue.try_pop(que_value))
        {
            v.push_back(que_value);
            pushed_items--;
        }
            
    }
    value = std::accumulate(v.begin(), v.end(), 0);
}

int tester::run()
{
    std::vector<int> results_in(in_threads_count, 0);
    std::vector<std::thread> threads_in;

    std::vector<int> results_out(out_threads_count, 0);
    std::vector<std::thread> threads_out;

    for(int i = 0; i < in_threads_count; i++)
        threads_in.emplace_back(std::move(std::thread(&tester::spam_values_in, this, std::ref(results_in.at(i))))); 

    for(int i = 0; i < out_threads_count; i++)
        threads_out.emplace_back(std::move(std::thread(&tester::spam_values_out, this, std::ref(results_out.at(i))))); 

    for(auto &a : threads_in)
        a.join();

    for(auto &a : threads_out)
        a.join();
    
    int value_in = std::accumulate(results_in.begin(), results_in.end(), 0);
    //std::cout << value_in << std::endl;
    int value_out = std::accumulate(results_out.begin(), results_out.end(), 0);
    //std::cout << value_in << std::endl;
    return value_in - value_out;
}


void test()
{
    int queue_size = (rand() % 15) + 1;
    int th_in = (rand() % 6) + 1;
    int th_out = (rand() % (7 - th_in)) + 1;
    int count = (rand() % (100000)) + 1;

    th_in = 1;
    th_out = 1;

    tester test(queue_size, th_in, th_out, count);
    
    int test_result = test.run();
    if(test_result != 0)
    {
        std::cout << "threads_in: " << th_in << std::endl;
        std::cout << "threads_out: " << th_out << std::endl;
        std::cout << "queue_size: " << (1 << queue_size) << std::endl;
        std::cout << "test count: " << count << std::endl;
        std::cout << test_result << std::endl;
        std::cout << std::endl;
    }
}


int main()
{
    for(int i = 0; i < 1000; ++i)
        test();
}



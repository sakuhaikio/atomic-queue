# atomic_queue

A lightweight lock-free queue implemented using atomic operations.

The queue provides a simple non-blocking interface suitable for concurrent
producer/consumer workloads. Operations never block; instead they return
a boolean indicating whether the operation succeeded.

The implementation is designed to be small, dependency-free, and easy to
integrate into performance-critical systems.

---

# Features

- Lock-free queue implementation
- Non-blocking `try_push` and `try_pop`
- Header-only usage

---

# Example

```cpp
#include "queue.hpp"
#include <iostream>

int main()
{
    queue<int> q(64);

    q.try_push(1);
    q.try_push(2);

    int value;

    while(q.try_pop(value))
    {
        std::cout << value << std::endl;
    }
}

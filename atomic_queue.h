#include <atomic>
#include <concepts>
#include <iostream>

constexpr int CACHE_LINE_SIZE = 64;

class AB_lock_group
{
public:
    void lock_A();
    void lock_B();
    void unlock_A();
    void unlock_B();
private:
    std::atomic_int counter = 0;
};

void AB_lock_group::lock_A()
{
    int v = counter.load();
    while(true)
        if (v < 0)
        {
            v = counter.load();
            continue;
        }
        else if(counter.compare_exchange_strong(v, v + 1))
            break;
}

void AB_lock_group::lock_B()
{
    int v = counter.load();
    while(true)
        if (v > 0)
        {
            v = counter.load();
            continue;
        }
        else if(counter.compare_exchange_strong(v, v - 1))
            break;
}

void AB_lock_group::unlock_A()
{
    counter.fetch_sub(1);
}

void AB_lock_group::unlock_B()
{
    counter.fetch_add(1);
}

template<typename T>
class atomic_queue
{
    struct container
    {
        container();
        ~container();
        template<typename... Args>
        constexpr void add_data(Args&&... args);
        constexpr void destroy_data();
        constexpr T&& move();

        alignas(alignof(T)) unsigned char data[sizeof(T)];
    };

public:

    atomic_queue(const unsigned size = 64);
    ~atomic_queue() noexcept;

    atomic_queue() noexcept = delete;
    atomic_queue(atomic_queue<T> const&) = delete;
    atomic_queue<T>& operator=(atomic_queue<T> const&) = delete;

    constexpr bool try_push(const T& t) noexcept { return emplace(t); };
    constexpr bool try_push(std::convertible_to<T> auto&& t) noexcept { return emplace(t); };
    bool try_pop(T& value);

//private:

    template<typename... Args>
    bool emplace(Args&&... args);

    alignas(CACHE_LINE_SIZE) std::atomic_uint read_;
    alignas(CACHE_LINE_SIZE) std::atomic_uint write_;
    alignas(CACHE_LINE_SIZE) std::atomic_uint reserved_read_;
    alignas(CACHE_LINE_SIZE) std::atomic_uint reserved_write_;

    alignas(CACHE_LINE_SIZE) container* buffer_;
    unsigned size_;

    AB_lock_group A_lock;
    AB_lock_group B_lock;

};

template<typename T>
atomic_queue<T>::atomic_queue(const unsigned size) :
    read_(0), write_(0), reserved_read_(0), reserved_write_(0)
{
    size_ = (size | 0xF) & 0xFFFF;
    for(unsigned i = 1; i <= 16; i *= 2)
        size_ |= size_ >> i;
    //size_ = (1<<2) - 1;

    buffer_ = new container[size_ + 1];
}

template<typename T>
atomic_queue<T>::~atomic_queue() noexcept
{
    while(read_ != write_)
        buffer_[read_.exchange((read_.load(std::memory_order_relaxed) + 1) & size_, 
            std::memory_order_relaxed)].destroy_data();
        
    delete[] buffer_;
}

template<typename T> atomic_queue<T>::container::container()
{
}

template<typename T>
atomic_queue<T>::container::~container()
{
}

template<typename T>
template<typename... Args>
constexpr void atomic_queue<T>::container::add_data(Args&&... args)
{
    new(&data) T(std::forward<Args>(args)...);
}

template<typename T>
constexpr void atomic_queue<T>::container::destroy_data()
{
    reinterpret_cast<T*>(&data)->~T();
}

template<typename T>
constexpr T&& atomic_queue<T>::container::move()
{
    return reinterpret_cast<T&&>(data);
}

template<typename T>
template<typename... Args>
bool atomic_queue<T>::emplace(Args&&... args)
{
    unsigned current_write = reserved_write_.load();
    while(true)
    {

        A_lock.lock_A();
        if(((current_write + 1) & size_) == read_.load())
        {
            A_lock.unlock_A();
            return false;
        }
        auto b = reserved_write_.compare_exchange_strong(current_write, (current_write + 1) & size_);
        A_lock.unlock_A();

        if(b) break;
    }

    buffer_[current_write].add_data(std::forward<Args>(args)...);

    while(true)
    {
        unsigned t = current_write;

        B_lock.lock_A();
        auto b = write_.compare_exchange_strong(t, (current_write + 1) & size_);
        B_lock.unlock_A();

        if(b) return true;
    }
}

template<typename T>
bool atomic_queue<T>::try_pop(T& value)
{
    unsigned current_read = reserved_read_.load();
    while(true)
    {
        B_lock.lock_B();
        if(current_read == write_.load())
        {
            B_lock.unlock_B();
            return false;
        }
        auto b = reserved_read_.compare_exchange_strong(current_read, (current_read + 1) & size_);
        B_lock.unlock_B();

        if(b) break;
    }

    container* can = buffer_ + current_read;
    value = std::move(can->move());
    can->destroy_data();

    while(true)
    {
        unsigned t = current_read;

        A_lock.lock_B();
        auto b = read_.compare_exchange_strong(t, (current_read + 1) & size_);
        A_lock.unlock_B();

        if(b) return true;
    }
}

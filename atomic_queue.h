#include <atomic>
#include <concepts>

constexpr int CACHE_LINE_SIZE = 64;

template<typename T>
class atomic_queue
{
    template<typename U>
    struct container
    {
        container();
        ~container();
        template<typename... Args>
        constexpr void add_data(Args&&... args);
        constexpr void destroy_data();
        constexpr U&& move();

        alignas(alignof(U)) unsigned char data[sizeof(U)];
    };

public:

    atomic_queue(const size_t size = 64);
    ~atomic_queue() noexcept;
    constexpr bool try_push(const T& t) noexcept { return emplace(t); };
    constexpr bool try_push(std::convertible_to<T> auto&& t) noexcept { return emplace(t); };
    constexpr bool try_pop(T& value);

private:

    template<typename... Args>
    bool emplace(Args... args);

    alignas(CACHE_LINE_SIZE) std::atomic_uint read_;
    alignas(CACHE_LINE_SIZE) std::atomic_uint write_;
    alignas(CACHE_LINE_SIZE) std::atomic_uint reserved_read_;
    alignas(CACHE_LINE_SIZE) std::atomic_uint reserved_write_;

    alignas(CACHE_LINE_SIZE) container<T>* buffer_;
    const size_t size_;
};

template<typename T>
atomic_queue<T>::atomic_queue(const size_t size) :
    buffer_(new container<T>[size]),
    read_(0), write_(0), reserved_read_(0), reserved_write_(0),
    size_(size-1)
{
}

template<typename T>
atomic_queue<T>::~atomic_queue() noexcept
{
    for(int i = 0; i < size_ + 1; ++i)
        buffer_[i].~container();
    delete[] buffer_;
}

template<typename T>
template<typename U>
atomic_queue<T>::container<U>::container()
{
}

template<typename T>
template<typename U>
atomic_queue<T>::container<U>::~container()
{
    destroy_data();
}

template<typename T>
template<typename U>
template<typename... Args>
constexpr void atomic_queue<T>::container<U>::add_data(Args&&... args)
{
    new(&data) U(std::forward<Args>(args)...);
}

template<typename T>
template<typename U>
constexpr void atomic_queue<T>::container<U>::destroy_data()
{
    reinterpret_cast<U*>(&data)->~U();
}

template<typename T>
template<typename U>
constexpr U&& atomic_queue<T>::container<U>::move()
{
    return reinterpret_cast<U&&>(data);
}

template<typename T>
template<typename... Args>
bool atomic_queue<T>::emplace(Args... args)
{
    unsigned current_write = reserved_write_.load(std::memory_order_relaxed);
    do
        if(((current_write + 1) & size_) == read_.load(std::memory_order_relaxed))
            return false;
    while(!reserved_write_.compare_exchange_strong(
        current_write, (current_write + 1) & size_,
        std::memory_order_acquire,
        std::memory_order_relaxed));

    buffer_[current_write].add_data(std::forward<Args>(args)...);

    unsigned t;
    do
        t = current_write;
    while(!write_.compare_exchange_strong(t, (current_write + 1) & size_,
        std::memory_order_relaxed,
        std::memory_order_relaxed));

    return true;
}

template<typename T>
constexpr bool atomic_queue<T>::try_pop(T& value)
{
    unsigned current_read = reserved_read_.load(std::memory_order_relaxed);
    do
        if(current_read == write_.load(std::memory_order_relaxed))
            return false;
    while(!reserved_read_.compare_exchange_strong(current_read, (current_read + 1) & size_,
        std::memory_order_acquire,
        std::memory_order_relaxed));

    container<T>* can = buffer_ + current_read;
    value = std::move(can->move());
    can->destroy_data();

    unsigned t;
    do
        t = current_read;
    while(!read_.compare_exchange_strong(t, (current_read + 1) & size_,
        std::memory_order_relaxed,
        std::memory_order_relaxed));

    return true;
}

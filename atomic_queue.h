#include <atomic>
#include <concepts>
#include <iostream>

constexpr int CACHE_LINE_SIZE = 64;

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

    std::atomic_flag flag_A = ATOMIC_FLAG_INIT;
    std::atomic_flag flag_B = ATOMIC_FLAG_INIT;

};

template<typename T>
atomic_queue<T>::atomic_queue(const unsigned size) :
    read_(0), write_(0), reserved_read_(0), reserved_write_(0)
{
    size_ = (size | 0xF) & 0xFFFF;
    for(unsigned i = 1; i <= 16; i *= 2)
        size_ |= size_ >> i;

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
    if(((current_write + 1) & size_) == read_.load())
        return false;
    reserved_write_.store((current_write + 1) & size_);

    buffer_[current_write].add_data(std::forward<Args>(args)...);

    write_.store((current_write + 1) & size_);
    return true;
}

template<typename T>
bool atomic_queue<T>::try_pop(T& value)
{
    unsigned current_read = reserved_read_.load();
    if(current_read == write_.load())
        return false;

    reserved_read_.store((current_read + 1) & size_);

    container* can = buffer_ + current_read;
    value = std::move(can->move());
    can->destroy_data();

    read_.store((current_read + 1) & size_);
    return true;
}

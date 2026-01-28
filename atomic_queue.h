#include <atomic>
#include <concepts>
#include <memory>
#include <new>

namespace atomic_ring
{

inline constexpr std::size_t CACHE_LINE_SIZE =
    std::hardware_destructive_interference_size;

constexpr uint32_t lo32(uint64_t n) noexcept 
{
    return uint32_t(n); 
}

constexpr uint32_t hi32(uint64_t n) noexcept
{
    return uint32_t(n >> 32); 
}

constexpr uint64_t pack_u32x2(uint32_t lo, uint32_t hi) noexcept
{
    return (uint64_t(hi) << 32) | uint64_t(lo);
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

        alignas(alignof(T)) std::byte data[sizeof(T)];
    };

public:

    explicit atomic_queue(const uint32_t size = 64);
    atomic_queue() noexcept = delete;
    atomic_queue(atomic_queue<T> const&) = delete;
    atomic_queue<T>& operator=(atomic_queue<T> const&) = delete;

    ~atomic_queue() noexcept;

    constexpr bool try_push(const T& t) noexcept { return emplace(t); };
    constexpr bool try_push(std::convertible_to<T> auto&& t) noexcept { return emplace(t); };
    bool try_pop(T& value);

private:

    template<typename... Args>
    bool emplace(Args&&... args);

    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> r_rw_;
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> rr_w_;

    std::unique_ptr<container[]> buffer_;
    uint32_t size_;
};

template<typename T>
atomic_queue<T>::atomic_queue(const unsigned size) :
    r_rw_(0), rr_w_(0)
{
    size_ = (size | 0xF) & 0xFFFF;
    for(unsigned i = 1; i <= 16; i *= 2)
        size_ |= size_ >> i;

    buffer_ = std::make_unique<container[]>(size_ + 1);
}

template<typename T>
atomic_queue<T>::~atomic_queue() noexcept
{
    const uint32_t w = hi32(rr_w_.load(std::memory_order_relaxed));
    for(uint32_t r = lo32(r_rw_.load(std::memory_order_relaxed)); r != w; r = ++r & size_)
        buffer_[r].destroy_data();
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
    ::new(&data) T(std::forward<Args>(args)...);
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
    // move write reservation
    uint64_t r_rw = r_rw_.load(std::memory_order_relaxed);
    uint32_t r, rw;
    do
    {
        r = lo32(r_rw);
        rw = hi32(r_rw);

        if(((rw + 1) & size_) == r)
            return false;
    }
    while(!r_rw_.compare_exchange_strong(
        r_rw, pack_u32x2(r, (rw + 1) & size_),
        std::memory_order_acquire,
        std::memory_order_relaxed));

    // write data 
    buffer_[rw].add_data(std::forward<Args>(args)...);


    // move write finished
    uint64_t rr_w = rr_w_.load(std::memory_order_relaxed);
    uint32_t rr;
    do
    {
        rr = lo32(rr_w);
        rr_w = pack_u32x2(rr, rw);
    }
    while(!rr_w_.compare_exchange_strong(
        rr_w, pack_u32x2(rr, (rw + 1) & size_),
        std::memory_order_release,
        std::memory_order_relaxed));

    return true;
}

template<typename T>
bool atomic_queue<T>::try_pop(T& value)
{
    // move read reservation
    uint64_t rr_w = rr_w_.load(std::memory_order_relaxed);
    uint32_t rr, w;
    do
    {
        rr = lo32(rr_w);
        w = hi32(rr_w);

        if(rr == w)
            return false;
    }
    while(!rr_w_.compare_exchange_strong(
        rr_w, pack_u32x2((rr + 1) & size_, w),
        std::memory_order_acquire,
        std::memory_order_relaxed));

    // read data
    container* can = buffer_.get() + rr;
    value = std::move(can->move());
    can->destroy_data();

    // move read finished
    uint64_t r_rw = r_rw_.load(std::memory_order_relaxed);
    uint32_t rw;
    do
    {
        rw = hi32(r_rw);
        r_rw = pack_u32x2(rr, rw);
    }
    while(!r_rw_.compare_exchange_strong(
        r_rw, pack_u32x2((rr + 1) & size_, rw),
        std::memory_order_release,
        std::memory_order_relaxed));

    return true;
}

}

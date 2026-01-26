#include <atomic>
#include <concepts>
#include <memory>

namespace atomic_ring
{

constexpr int CACHE_LINE_SIZE = 64;

constexpr uint32_t lo32(uint64_t n) noexcept 
{
    return uint32_t(n); 
}

constexpr uint32_t hi32(uint64_t n) noexcept
{
    return uint32_t(n >> 32); 
}

constexpr uint64_t pack64(uint32_t lo, uint32_t hi) noexcept
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

        alignas(alignof(T)) uint8_t data[sizeof(T)];
    };

public:

    atomic_queue(const uint32_t size = 64);
    ~atomic_queue() noexcept;

    atomic_queue() noexcept = delete;
    atomic_queue(atomic_queue<T> const&) = delete;
    atomic_queue<T>& operator=(atomic_queue<T> const&) = delete;

    constexpr bool try_push(const T& t) noexcept { return emplace(t); };
    constexpr bool try_push(std::convertible_to<T> auto&& t) noexcept { return emplace(t); };
    bool try_pop(T& value);

private:

    template<typename... Args>
    bool emplace(Args&&... args);

    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> rr_w_bits;
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> rw_r_bits;

    alignas(CACHE_LINE_SIZE) std::unique_ptr<container[]> buffer_;
    uint32_t size_;
};

template<typename T>
atomic_queue<T>::atomic_queue(const unsigned size) :
    rw_r_bits(0), rr_w_bits(0)
{
    size_ = (size | 0xF) & 0xFFFF;
    for(unsigned i = 1; i <= 16; i *= 2)
        size_ |= size_ >> i;

    buffer_ = std::make_unique<container[]>(size_ + 1);
}

template<typename T>
atomic_queue<T>::~atomic_queue() noexcept
{
//    while(rr_w_.load(std::memory_order_relaxed).b != rw_r_.load(std::memory_order_relaxed).b)
//        buffer_[rw_r_.exchange({rw_r_.load(std::memory_order_relaxed).a, (rw_r_.load(std::memory_order_relaxed).b + 1) & size_}, 
//            std::memory_order_relaxed).b].destroy_data();
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
    uint64_t curr_rw_r = rw_r_bits.load(std::memory_order_relaxed);
    uint32_t rw; uint32_t r;
    do
    {
        rw = lo32(curr_rw_r);
        r = hi32(curr_rw_r);
        if(((rw + 1) & size_) == r)
            return false;
    }
    while(! rw_r_bits.compare_exchange_strong(
        curr_rw_r, 
        pack64(rw + 1 & size_, r),
        std::memory_order_acquire,
        std::memory_order_relaxed));

    buffer_[rw].add_data(std::forward<Args>(args)...);

    uint64_t curr_rr_w = rr_w_bits.load(std::memory_order_relaxed);
    uint32_t rr; uint32_t w;
    do 
    {
        rr = lo32(curr_rr_w);
        w = rw;
    }
    while(!rr_w_bits.compare_exchange_strong(
        curr_rr_w, 
        pack64(rr, (w + 1) & size_),
        std::memory_order_release,
        std::memory_order_relaxed));

    return true;
}

template<typename T>
bool atomic_queue<T>::try_pop(T& value)
{
    uint64_t curr_rr_w = rr_w_bits.load(std::memory_order_relaxed);
    uint32_t rr; uint32_t w;
    do
    {
        rr = lo32(curr_rr_w);
        w = hi32(curr_rr_w);
        if(rr == w)
            return false;
    }
    while(! rr_w_bits.compare_exchange_strong(
        curr_rr_w, pack64((rr + 1) & size_, w),
        std::memory_order_acquire,
        std::memory_order_relaxed));

    container* can = buffer_.get() + rr;
    value = std::move(can->move());
    can->destroy_data();

    uint64_t curr_rw_r = rw_r_bits.load(std::memory_order_relaxed);
    uint32_t rw; uint32_t r;
    do 
    {
        rw = lo32(curr_rw_r);
        r = rr;
    }
    while(!rw_r_bits.compare_exchange_strong(
        curr_rw_r, pack64(rw, (r + 1) & size_),
        std::memory_order_release,
        std::memory_order_relaxed));

    return true;
}

}

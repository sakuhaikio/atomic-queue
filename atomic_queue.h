#include <atomic>
#include <concepts>

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

    struct S
    {
        unsigned a;
        unsigned b;
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

private:

    template<typename... Args>
    bool emplace(Args&&... args);

    alignas(CACHE_LINE_SIZE) std::atomic<S> rr_w_;
    alignas(CACHE_LINE_SIZE) std::atomic<S> rw_r_;

    alignas(CACHE_LINE_SIZE) container* buffer_;

    unsigned size_;
};

template<typename T>
atomic_queue<T>::atomic_queue(const unsigned size) :
    rr_w_({0, 0}), rw_r_({0, 0})
{
    size_ = (size | 0xF) & 0xFFFF;
    for(unsigned i = 1; i <= 16; i *= 2)
        size_ |= size_ >> i;

    buffer_ = new container[size_ + 1];
}

template<typename T>
atomic_queue<T>::~atomic_queue() noexcept
{
    while(rr_w_.load().b != rw_r_.load().b)
        buffer_[rw_r_.exchange({rw_r_.load().a, (rw_r_.load().b + 1) & size_}, 
            std::memory_order_relaxed).b].destroy_data();
        
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
    S curr_rw_r = rw_r_.load();
    do
        if(((curr_rw_r.a + 1) & size_) == curr_rw_r.b)
            return false;
    while(! rw_r_.compare_exchange_strong(
        curr_rw_r, 
        {(curr_rw_r.a + 1) & size_, curr_rw_r.b}));

    buffer_[curr_rw_r.a].add_data(std::forward<Args>(args)...);

    S t = rr_w_.load();
    do t.b = curr_rw_r.a;
    while(!rr_w_.compare_exchange_strong(
        t, {t.a, (t.b + 1) & size_}));

    return true;
}

template<typename T>
bool atomic_queue<T>::try_pop(T& value)
{
    S curr_rr_w = rr_w_.load();
    do
        if(curr_rr_w.a == curr_rr_w.b)
            return false;
    while(! rr_w_.compare_exchange_strong(
        curr_rr_w, 
        {(curr_rr_w.a + 1) & size_, curr_rr_w.b}));

    container* can = buffer_ + curr_rr_w.a;
    value = std::move(can->move());
    can->destroy_data();

    S t = rw_r_.load();
    do t.b = curr_rr_w.a;
    while(!rw_r_.compare_exchange_strong(
        t, {t.a, (t.b + 1) & size_}));

    return true;
}

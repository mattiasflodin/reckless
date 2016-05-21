#include <cstdlib>
#include <cstdint>

#if defined(__GNUC__)
#include <immintrin.h>    // _mm_pause
#elif defined(_MSC_VER)
#include <intrin.h>    // _mm_pause
#else
#error Unknown compiler, need #include for _mm_pause
#endif

#include "utility.hpp"  // atomic_*
#include "branch_hints.hpp" // likely

namespace reckless {
namespace detail {

class mpsc_ring_buffer {
public:
    mpsc_ring_buffer() :
#if defined(_WIN32)
        mapping_handle_(0),
#endif
        next_write_position_(0),
        next_read_position_cached_(0),
        next_read_position_(0),
        pbuffer_start_(nullptr),
        capacity_(0)
    {
    }

    mpsc_ring_buffer(std::size_t capacity)
    {
        init(capacity);
    }

    ~mpsc_ring_buffer()
    {
        destroy();
    }

    void reserve(std::size_t capacity)
    {
        destroy();
        init(capacity);
    }

    void* push(std::size_t size) noexcept
    {
        auto capacity = capacity_;
        for(;;_mm_pause()) {
            auto wp = atomic_load_relaxed(&next_write_position_);
            auto rp = atomic_load_relaxed(&next_read_position_cached_);
            auto nwp = wp + size;
            if(likely(nwp - rp <= capacity)) {
            } else {
                auto rp_live = atomic_load_relaxed(&next_read_position_);
                if(atomic_compare_exchange_weak_relaxed(&next_read_position_cached_, rp, rp_live)) {
                    if(nwp - rp_live > capacity)
                        return nullptr;
                    rp = rp_live;
                } else {
                    continue;
                }
            }

            if(atomic_compare_exchange_weak_relaxed(&next_write_position_, wp, nwp))
                return pbuffer_start_ + (wp & (capacity-1));
        }
    }

    void* front() noexcept
    {
        return pbuffer_start_ + (next_read_position_ & (capacity_-1));
    }

    std::size_t size() noexcept
    {
        auto wp = atomic_load_relaxed(&next_write_position_);
        return wp - next_read_position_;
    }

    void pop_release(std::size_t size) noexcept
    {
        atomic_store_release(&next_read_position_, next_read_position_+size);
    }

    void pop_relaxed(std::size_t size) noexcept
    {
        atomic_store_relaxed(&next_read_position_, next_read_position_+size);
    }

private:
    void init(std::size_t capacity);
    void destroy();

#if defined(_WIN32)
    std::uintptr_t mapping_handle_;
#endif
    std::uint64_t next_write_position_;
    std::uint64_t next_read_position_cached_;
    std::uint64_t next_read_position_;
    char* pbuffer_start_;
    std::size_t capacity_;
};

}   // namespace detail
}   // namespace reckless

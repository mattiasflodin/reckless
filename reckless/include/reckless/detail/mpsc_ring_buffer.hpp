#include <cstdlib>
#include <cstdint>

#if defined(__GNUC__)
#include <immintrin.h>    // _mm_pause
#elif defined(_MSC_VER)
#include <intrin.h>    // _mm_pause
#else
#error Unknown compiler, need #include for _mm_pause
#endif

#if defined(_WIN32)
#include <Windows.h>    // InterlockedCompareExchangeNoFence64
#endif

// Chapter 7,  Part 3A - System Programming Guide of Intel's processor manuals:
// 8.1.1 Guaranteed Atomic Operations
// The Pentium processor (and newer processors since) guarantees that the following
// additional memory operations will always be carried out atomically :
// * Reading or writing a quadword aligned on a 64-bit boundary
//
// On IA-32 the VC++ stdlib goes through many hoops with interlocked Ors, loops etc
// to achieve atomicity when it really isn't necessary.
inline std::uint64_t atomic_load_relaxed(std::uint64_t& value)
{
    return value;
}

inline void atomic_store_relaxed(std::uint64_t* ptarget, std::uint64_t value)
{
    *ptarget = value;
}

inline bool atomic_compare_exchange_weak_relaxed(std::uint64_t* ptarget, std::uint64_t expected, std::uint64_t desired)
{
#if defined(__GNUC__)
    return __atomic_compare_exchange_n(ptarget, &expected, desired, true,
            __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#elif defined(_WIN32) // TODO change to _MSC_VER and skip windows.h
    return static_cast<LONGLONG const&>(expected) ==
        InterlockedCompareExchangeNoFence64(
            reinterpret_cast<LONGLONG*>(ptarget),
            reinterpret_cast<LONGLONG&>(desired),
            reinterpret_cast<LONGLONG&>(expected));
#endif
}

class mpsc_ring_buffer {
public:
    mpsc_ring_buffer(std::size_t capacity);
    ~mpsc_ring_buffer();

    void* push_back(std::size_t size)
    {
        auto capacity = capacity_;
        for(;;_mm_pause()) {
            auto wp = atomic_load_relaxed(next_write_position_);
            auto rp = atomic_load_relaxed(next_read_position_cached_);
            auto nwp = wp + size;
            if(nwp - rp <= capacity) {
                // Likely case.
            } else {
                auto rp_live = atomic_load_relaxed(next_read_position_);
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

    void* front()
    {
        return pbuffer_start_ + (next_read_position_ & (capacity_-1));
    }

    std::size_t size()
    {
        auto wp = atomic_load_relaxed(next_write_position_);
        return wp - next_read_position_;
    }

    void pop_front(std::size_t size)
    {
        atomic_store_relaxed(&next_read_position_, next_read_position_+size);
    }

private:
#if defined(_WIN32)
    std::uintptr_t mapping_handle_;
#endif
    std::uint64_t next_write_position_ = 0;
    std::uint64_t next_read_position_cached_ = 0;
    std::uint64_t next_read_position_ = 0;
    char* pbuffer_start_;
    std::size_t const capacity_;
};

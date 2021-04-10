/* This file is part of reckless logging
 * Copyright 2015-2020 Mattias Flodin <git@codepentry.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "platform.hpp"  // likely, atomic_*, pause

#include <cstdlib>  // size_t
#include <cstdint>  // uint64_t, uintptr_t
#include <cstring>  // memset

namespace reckless {
namespace detail {

// This is a lock-free, multiple-producer, single-consumer "magic ring buffer":
// a ring buffer / circular buffer that takes advantage of the virtual memory
// system to map the same physical memory twice to consecutive memory addresses.
// This avoids the added complexity of having to deal with buffer wraparound in
// the middle of an allocated block. Furthermore, it uses 64-bit offsets that
// never wrap to represent the read and write positions of the buffer. That is,
// the read position is always less than or equal to the write position, hence
// requiring only a simple subtraction to determine how much buffer space is
// currently occupied. More importantly, doing this avoids the ABA problem. The
// final address to use for actual memory access is computed by a modulo
// operation once a block has been allocated. We assume that the 64-bit position
// variables will never overflow. A process that writes 100 GiB every second
// would take five years for this to happen.
class mpsc_ring_buffer {
public:
    mpsc_ring_buffer()
    {
        rewind();
        pbuffer_start_ = nullptr;
        capacity_ = 0;
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
        for(;;pause()) {
            auto wp = atomic_load_relaxed(&next_write_position_);
            auto rp = atomic_load_relaxed(&next_read_position_);
            auto nwp = wp + size;
            if(likely(nwp - rp <= capacity)) {
            } else {
                return nullptr;
            }

            if(atomic_compare_exchange_weak_relaxed(&next_write_position_, wp, nwp))
                return pbuffer_start_ + (wp & (capacity-1));
        }
    }

    // Allocate all remaining space, filling the buffer to its capacity.
    void deplete() noexcept
    {
        auto capacity = capacity_;
        for(;;pause()) {
            auto wp = atomic_load_relaxed(&next_write_position_);
            auto rp = atomic_load_relaxed(&next_read_position_);
            auto used = wp - rp;
            auto remaining = capacity - used;
            auto nwp = wp + remaining;
            if(atomic_compare_exchange_weak_relaxed(&next_write_position_, wp, nwp))
                return;
        }
    }

    std::uint64_t read_position() noexcept
    {
        return next_read_position_;
    }

    void* address(std::uint64_t position) noexcept
    {
        return pbuffer_start_ + (position & (capacity_ - 1));
    }

    std::size_t size() noexcept
    {
        auto wp = atomic_load_relaxed(&next_write_position_);
        return static_cast<std::size_t>(wp - next_read_position_);
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
    void rewind()
    {
        next_write_position_ = 0;
        next_read_position_ = 0;
    }

    // To avoid false sharing that triggers unnecessary cache-line
    // ping pong we segment these variables by how they are accessed.
    // First, the variables that are only read by both consumer and producer.
    char padding1_[RECKLESS_CACHE_LINE_SIZE];
    char* pbuffer_start_;
    std::size_t capacity_;
    char padding2_[RECKLESS_CACHE_LINE_SIZE
        - sizeof(char*) - sizeof(std::size_t)];

    // Next, variables that are updated by the producer and read by
    // the consumer. Strictly the consumer does not access
    // next_read_position_cached_ at all, but when it is updated it
    // always happens together with next_write_position_ anyway.
    std::uint64_t next_write_position_;
    char padding3_[RECKLESS_CACHE_LINE_SIZE - 1*8];

    // Finally, next_read_position_ is updated by the consumer and
    // somtimes read by the producer.
    std::uint64_t next_read_position_;
    char padding4_[RECKLESS_CACHE_LINE_SIZE - 8];
};

}   // namespace detail
}   // namespace reckless

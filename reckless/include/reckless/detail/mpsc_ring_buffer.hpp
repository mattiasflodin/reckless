/* This file is part of reckless logging
 * Copyright 2015, 2016 Mattias Flodin <git@codepentry.com>
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
#include <cstdlib>
#include <cstdint>  // uint64_t, uintptr_t

#include "platform.hpp"  // likely, atomic_*, pause

namespace reckless {
namespace detail {

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
        next_read_position_cached_ = 0;
        next_read_position_ = 0;
    }

    std::uint64_t next_write_position_;
    std::uint64_t next_read_position_cached_;
    std::uint64_t next_read_position_;
    char* pbuffer_start_;
    std::size_t capacity_;
};

}   // namespace detail
}   // namespace reckless

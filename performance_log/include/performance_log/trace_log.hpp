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
#ifndef PERFORMANCE_LOG_TRACE_LOG_HPP
#define PERFORMANCE_LOG_TRACE_LOG_HPP
#include <cstddef>      // size_t, max_align_t
#include <string>
#include <type_traits>  // is_empty
#include <mutex>        // mutex, unique_lock
#include <algorithm>    // move
#include <sstream>      // ostringstream
#include <set>
#include <cassert>
#include <cstring>

#include <reckless/detail/platform.hpp>

namespace reckless {
namespace detail {

class trace_log {
public:
    trace_log(std::size_t size) :
        pbuffer_(new char[size]),
        pnext_event_(pbuffer_),
        pbuffer_end_(pbuffer_ + size)
    {
        std::memset(pbuffer_, 0, size);
    }

    template <class Callback>
    void read(Callback const& callback)
    {
        if (pnext_event_ > pbuffer_end_) {
            throw std::logic_error(
                "Events have been written past end of buffer");
        }
        void const* p = pbuffer_;
        std::string text;
        while(p != pnext_event_) {
            event_header const* pheader = static_cast<event_header const*>(p);
            auto pcevent = static_cast<char const*>(p)
                + aligned_sizeof<event_header>();
            p = pheader->pconsume_event(pcevent, &text);

            callback(text);
        }
    }

    void save(std::ostream& os)
    {
        read([&](std::string const& s) {
            os << s << std::endl;
        });
    }

    template <class Event, class... Args>
    void log_event(Args&&... args)
    {
        auto const record_size = aligned_sizeof<event_header>() + (std::is_empty<Event>()?
            0 : aligned_sizeof<Event>());
        char* pnext_event = atomic_fetch_add_relaxed(&pnext_event_, record_size);
        auto pevent_header = static_cast<event_header*>(
            static_cast<void*>(pnext_event));
        auto pevent = pnext_event + aligned_sizeof<event_header>();
        pevent_header->pconsume_event = &consume_event<Event>;
        new (pevent) Event(std::forward<Args>(args)...);
    }

private:
    struct event_header {
        void const* (*pconsume_event)(void const*, std::string*);
    };

    static std::size_t align_size(std::size_t size)
    {
        std::size_t const alignment = alignof(std::max_align_t);
        return ((size + alignment - 1) / alignment)*alignment;
    }

    template <class T>
    static std::size_t aligned_sizeof()
    {
        return align_size(sizeof(T));
    }

    template <class Event>
    static void const* consume_event(void const* pvevent, std::string* ptext)
    {
        auto pevent = static_cast<Event const*>(pvevent);
        *ptext = pevent->format();
        pevent->~Event();

        auto pcevent = static_cast<char const*>(pvevent);
        if(std::is_empty<Event>())
            return pcevent;
        else
            return pcevent + aligned_sizeof<Event>();
    }

    char* pbuffer_;
    char* pnext_event_;
    char* pbuffer_end_;
};

struct timestamped_trace_event {
    std::uint64_t timestamp;

    timestamped_trace_event() : timestamp(rdtsc())
    {
    }

    std::string format() const
    {
        std::ostringstream ostr;
        ostr << std::hex << timestamp;
        return ostr.str();
    }
};

extern trace_log g_trace_log;
#define RECKLESS_TRACE(Event, ...) reckless::detail::g_trace_log.log_event<Event>(__VA_ARGS__)

}   // namespace detail
}   // namespace reckless

#endif // PERFORMANCE_LOG_TRACE_LOG_HPP

#ifndef RECKLESS_DETAIL_TRACE_LOG_HPP
#define RECKLESS_DETAIL_TRACE_LOG_HPP
#include <cstddef>      // size_t, max_align_t
#include <string>
#include <type_traits>  // is_empty
#include <thread>       // this_thread
#include <mutex>        // mutex, unique_lock
#include <algorithm>    // move
#include <sstream>      // ostringstream
#include <set>
#include <cassert>

#include "platform.hpp"

namespace reckless {
namespace detail {

class trace_log {
public:
    trace_log(std::size_t size) :
        pbuffer_(new char[size]),
        pnext_event_(pbuffer_),
        pbuffer_end_(pbuffer_ + size),
        start_time_(rdtsc())
    {
    }

    template <class Callback>
    void read(Callback const& callback) const
    {
        std::ostringstream ostr;
        void const* p = pbuffer_;
        std::string text;
        while(p != pnext_event_) {
            event_header const* pheader = static_cast<event_header const*>(p);
            auto pcevent = static_cast<char const*>(p)
                + aligned_sizeof<event_header>();
            p = pheader->pconsume_event(pcevent, &text);

            ostr.str("");
            ostr << (pheader->timestamp - start_time_) << '\t'
                << pheader->thread_id << '\t'
                << move(text);
            callback(ostr.str());
        }
    }

    template <class Event, class... Args>
    void log_event(Args&&... args)
    {
        char* pnext_event;
        auto const record_size = sizeof(event_header) + (std::is_empty<Event>()?
            0 : aligned_sizeof<Event>());
        for(;;pause()) {
            pnext_event = atomic_load_relaxed(&pnext_event_);
            if(atomic_compare_exchange_weak_relaxed(&pnext_event_, pnext_event,
                pnext_event + record_size))
            {
                break;
            }
        }

        auto pevent_header = static_cast<event_header*>(
            static_cast<void*>(pnext_event));
        auto pevent = pnext_event + aligned_sizeof<event_header>();
        pevent_header->timestamp = rdtsc();
        pevent_header->pconsume_event = &consume_event<Event>;
        pevent_header->thread_id = std::this_thread::get_id();
        new (pevent) Event(std::forward<Args>(args)...);
        assert(pevent + aligned_sizeof<Event>() <= pbuffer_end_);
    }

private:
    struct event_header {
        std::uint64_t timestamp;
        void const* (*pconsume_event)(void const*, std::string*);
        std::thread::id thread_id;
    };

    template <class T>
    static std::size_t aligned_sizeof()
    {
        std::size_t const alignment = alignof(std::max_align_t);
        return ((sizeof(T) + alignment - 1) / alignment)*alignment;
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
    std::uint64_t start_time_;
};

#ifdef RECKLESS_ENABLE_TRACE_LOG
extern trace_log g_trace_log;
#endif

}   // namespace detail
}   // namespace reckless

#endif // RECKLESS_DETAIL_TRACE_LOG_HPP

#include <reckless/detail/spsc_event.hpp>
#include <new>
#include <Windows.h>

namespace reckless {
namespace detail {

spsc_event::spsc_event() : handle_(CreateEvent(NULL, FALSE, FALSE, NULL))
{
    if(handle_ == NULL)
        throw std::bad_alloc();
}

spsc_event::~spsc_event()
{
    CloseHandle(handle_);
}

}
}
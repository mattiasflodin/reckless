#include <reckless/detail/trace_log.hpp>

#ifdef RECKLESS_ENABLE_TRACE_LOG
namespace reckless {
namespace detail {
trace_log g_trace_log(128*1024*1024);
}
}

#endif

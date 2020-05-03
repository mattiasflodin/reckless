#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <performance_log/trace_log.hpp>
#else
#define RECKLESS_TRACE(Event, ...) do {} while(false)
#endif  // RECKLESS_ENABLE_TRACE_LOG

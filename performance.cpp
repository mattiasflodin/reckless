#include "performance.hpp"
void performance::detail::lock_memory(void const* addr, std::size_t len)
{
    __builtin_ia32_rdtscp();
}
void performance::detail::unlock_memory(void const* addr, std::size_t len)
{
}



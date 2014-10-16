#include "performance.hpp"

#include <system_error>

#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

void performance_log::detail::lock_memory(void const* addr, std::size_t len)
{
    if(0 != mlock(addr, len))
        throw std::system_error(errno, std::system_category());
}
void performance_log::detail::unlock_memory(void const* addr, std::size_t len)
{
    munlock(addr, len);
}

void performance_log::rdtscp_cpuid_clock::bind_cpu(int cpu)
{
    int nprocessors = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    auto const size = CPU_ALLOC_SIZE(nprocessors);
    cpu_set_t* pcpuset = CPU_ALLOC(nprocessors);
    if(not pcpuset)
        throw std::bad_alloc();
    try {
        CPU_ZERO_S(size, pcpuset);
        CPU_SET_S(cpu, size, pcpuset);
        int res = pthread_setaffinity_np(pthread_self(), size, pcpuset);
        if(res != 0)
            throw std::system_error(res, std::system_category());
    } catch(...) {
        CPU_FREE(pcpuset);
        throw;
    }
}

void performance_log::rdtscp_cpuid_clock::unbind_cpu()
{
    int nprocessors = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    auto const size = CPU_ALLOC_SIZE(nprocessors);
    cpu_set_t* pcpuset = CPU_ALLOC(nprocessors);
    if(not pcpuset)
        throw std::bad_alloc();
    try {
        CPU_ZERO_S(size, pcpuset);
        for(int i=0; i!=nprocessors; ++i)
            CPU_SET_S(i, size, pcpuset);
        int res = pthread_setaffinity_np(pthread_self(), size,  pcpuset);
        if(res != 0)
            throw std::system_error(res, std::system_category());
    } catch(...) {
        CPU_FREE(pcpuset);
        throw;
    }
}

#include <reckless/detail/mpsc_ring_buffer.hpp>
#include <new>    // bad_alloc

#if defined(__linux__)
#include <sys/mman.h>   // mmap
#include <sys/ipc.h>    // shmget/shmat
#include <sys/shm.h>    // shmget/shmat
#include <sys/stat.h>   // S_IRUSR/S_IWUSR
#include <reckless/detail/utility.hpp>  // get_page_size
#endif

namespace {
    std::size_t round_capacity(std::size_t capacity)
    {
#if defined(__linux__)
        std::size_t const granularity = reckless::detail::get_page_size();
#elif defined(_WIN32)
        std::size_t const granularity = 64*1024;
#endif
        return ((capacity + granularity - 1)/granularity)*granularity;
    }
}

mpsc_ring_buffer::mpsc_ring_buffer(std::size_t capacity) :
    capacity_(round_capacity(capacity))
{
    capacity = capacity_;

#if defined(__linux__)
    int shm = shmget(IPC_PRIVATE, capacity, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(shm == -1)
        throw std::bad_alloc();

    void* pbase = nullptr;
    while(!pbase) {
        void* pbase = mmap(nullptr, 2*capacity, PROT_NONE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(MAP_FAILED == pbase)
            throw std::bad_alloc();
        munmap(pbase, 2*capacity);
        pbase = shmat(shm, pbase, 0);
        if(pbase) {
            if((void*)-1 == shmat(shm, static_cast<char*>(pbase) + capacity, 0))
            {
                shmdt(pbase);
                pbase = nullptr;
            }
        }
    }
    shmctl(shm, IPC_RMID, nullptr);
    pbuffer_start_ = static_cast<char*>(pbase);

#elif defined(_WIN32)
    HANDLE mapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, (capacity >> 31)>>1, capacity & 0xffffffff, nullptr);
    if(mapping == NULL)
        throw std::bad_alloc();

    void* pbase = nullptr;
    while(!pbase) {
        pbase = VirtualAlloc(0, 2*capacity, MEM_RESERVE, PAGE_NOACCESS);
        if (!pbase) {
            CloseHandle(mapping);
            throw std::bad_alloc();
        }
        VirtualFree(pbase, 0, MEM_RELEASE);
        pbase = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity,
                pbase);
        if(pbase) {
            if(!MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity,
                                static_cast<char*>(pbase) + capacity))
            {
                UnmapViewOfFile(pbase);
                pbase = nullptr;
            }
        }
    }
    mapping_handle_ = reinterpret_cast<std::uintptr_t&>(mapping);
    pbuffer_start_ = static_cast<char*>(pbase);
#endif
}

mpsc_ring_buffer::~mpsc_ring_buffer()
{
#if defined(__linux__)
    shmdt(pbuffer_start_ + capacity_);
    shmdt(pbuffer_start_);
#elif defined(_WIN32)
    asdj asd not done;
#endif
}

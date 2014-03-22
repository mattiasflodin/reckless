#include <atomic>
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

class spsc_event {
public:
    spsc_event() : signal_(1)
    {
    }

    void signal()
    {
        // TODO possible performance improvement: If we know nobody is waiting,
        // then we don't need to signal the futex.
        atomic_exchange_explicit(&signal_, 1, std::memory_order_release);
        sys_futex(&signal_, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }

    void wait(timespec const* timeout = nullptr)
    {
        int signal = atomic_exchange_explicit(&signal_, 0, std::memory_order_acquire);
        while(not signal) {
            sys_futex(&signal_, FUTEX_WAIT, 0, timeout, nullptr, 0);
            signal = atomic_exchange_explicit(&signal_, 0, std::memory_order_acquire);
        }
    }

private:
    int sys_futex(void *addr1, int op, int val1, struct timespec const *timeout,
            void *addr2, int val3)
    {
        return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
    }

    int atomic_exchange_explicit(int* pvalue, int new_value, std::memory_order order)
    {
        int res = new_value;
        asm volatile("xchg %0, %1\n\t"
                : "+r"(res), "+m"(*pvalue)
                :
                : "memory", "cc");
        return res;
    }

    int signal_;
};

#include <reckless/detail/lockless_cv.hpp>

#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>

namespace reckless {
namespace detail {

namespace {

int sys_futex(void *addr1, int op, int val1, struct timespec const *timeout,
        void *addr2, int val3)
{
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

}   // anonymous namespace

void lockless_cv::notify_all()
{
    atomic_fetch_add_release(&notify_count_, 1);
    sys_futex(&notify_count_, FUTEX_WAKE_PRIVATE, 0x7fffffff,
        nullptr, nullptr, 0);
}

void lockless_cv::wait(unsigned expected_notify_count)
{
    sys_futex(&notify_count_, FUTEX_WAIT_PRIVATE, expected_notify_count,
        nullptr, 0, 0);
}

void lockless_cv::wait(unsigned expected_notify_count, unsigned milliseconds)
{
    struct timespec timeout = {0, 0};
    timeout.tv_sec = milliseconds/1000;
    timeout.tv_nsec = static_cast<long>(milliseconds%1000)*1000000;

    sys_futex(&notify_count_, FUTEX_WAIT_PRIVATE,
        expected_notify_count, &timeout, 0, 0);
}

}   // namespace detail
}   // namespace reckless

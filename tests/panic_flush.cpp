#include <asynclog.hpp>
#include <memory>
#include <cstdio>

#include <signal.h>

asynclog::policy_log<> g_log;

volatile int t1_count=0;
volatile int t2_count=0;
void sigsegv_handler(int)
{
    int t1c = t1_count;
    int t2c = t2_count;
    g_log.panic_flush();
    std::printf("Last log entry for t1 should be >=%d. Last log entry for t2 should be >=%d\n", t1c, t2c);
}

int main()
{
    struct sigaction act;
    std::memset(&act, 0, sizeof(act));
    act.sa_handler = &sigsegv_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &act, nullptr);
    
    asynclog::file_writer writer("log.txt");
    g_log.open(&writer);

    std::thread thread([]()
    {
        for(int i=0;i!=1000000;++i) {
            g_log.write("t2: %d\n", i);
            t2_count = i;
        }
    });
        
    for(int i=0;i!=100000;++i) {
        g_log.write("t1: %d\n", i);
        t1_count = i;
    }
    char* p = nullptr;
    *p = 0;
    return 0;
}

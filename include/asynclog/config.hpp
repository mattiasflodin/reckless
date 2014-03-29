// 64 bytes is the common cache-line size on modern x86 CPUs, which
// avoids false sharing between the main thread and the output thread.
// TODO we could perhaps detect the cache-line size instead (?).
#define ASYNCLOG_ARCHITECTURE_CACHE_LINE_SIZE 64u

#define ASYNCLOG_USE_CXX11_THREAD_LOCAL

#define ASYNCLOG_ARCHITECTURE_PAGE_SIZE 4096u

#endif

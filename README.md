![Performance chart](doc/images/performance_periodic_calls.png)

Introduction
------------
Asynclog is an extremely low-latency, lightweight logging library. It
was created because I needed to perform extensive diagnostic logging
without worrying about performance. [Other logging
libraries](http://www.pantheios.org/performance.html) boast the ability
to throw log messages away very quickly; asynclog boasts the ability to
keep them all, without worrying about the performance impact. Filtering
can and should wait until you want to read the log, or need to clean up disk
space.

How it works
------------
By low latency I mean that the time from invoking the library and returning
to the caller is as short as I could make it. The code generated at the
call site consists of

1. Pushing the arguments on a thread-local queue. This has the same cost
   as pushing the arguments on the stack for a normal function call.
2. Call to Boost.Lockless (NB: this is bundled with the library, not an
   external dependency) to register the write request on a shared
   lockless queue.

The actual writing is performed asynchronously by a separate thread.
This removes or hides several costs:

* No transition to the kernel at the call site. The kernel is an easily
  overlooked but important cost, not only because the transition costs
  time, but because it pollutes the CPU cache. In other words, avoiding
  this *makes your non-logging code run faster* than if you were using a
  library that has to enter the kernel to perform its work.
* No locks need to be taken for synchronization between threads (unless
  the queue fills up; see the performance section for more information
  about the implications of this).
* It doesn't have to wait for the I/O operation to complete.
* If there are bursts of log calls, multiple items on the queue can be
  batched into a single write.

What's the catch?
-----------------
As all string formatting and I/O is done asynchronously and in a single
thread, there are a few caveats you need to be aware of:
* If you choose to pass log arguments by reference or pointer, then you
  must ensure that the referenced object remains valid at least until
  the log has been flushed or closed.
* You must take special care to handle crashes if you want to make sure
  that all log data prior to the crash is saved. This is not unique to
  asynchronous logging--for example fprintf will buffer data until you
  flush it--but asynchronous logging arguably makes the issue worse. The
  library provides convenience functions to aid with this.
* As all string formatting is done in a single thread, it could theoretically
  limit the scalability of your application if the formatting is very
  expensive.
* Performance becomes somewhat less predictable. Rather than putting the
  cost of the logging on the thread that calls the logging library, the
  OS may suspend some other thread to make room for the logging thread
  to run.

Basic use
---------
```c++
#include <asynclog.hpp>

// It is possible to build custom loggers for various ways of formatting the
// log. The severity log is a stock policy-based logger that allows you to
// configure fields that should be put on each line, including a severity
// marker for debug/info/warning/error.
using log_t = asynclog::severity_log<
    asynclog::indent<4>,       // 4 spaces of indent
    ' ',                       // Field separator
    asynclog::severity_field,  // Show severity marker (D/I/W/E) first
    asynclog::timestamp_field  // Then timestamp field
    >;
    
asynclog::file_writer writer("log.txt");
log_t g_log(&writer);

int main()
{
    std::string s("Hello World!");
    g_log.debug("Pointer: %p\n", s.c_str());
    g_log.info("Info line: %s\n", s);
    for(int i=0; i!=4; ++i) {
        asynclog::scoped_indent indent;  // The indent object causes the lines
        g_log.warn("Warning: %d\n", i);  // within this scope to be indented
    }
    g_log.error("Error: %f\n", 3.14);

    return 0;
}
```
This would give the following output:
```
D 2015-03-29 13:23:35.288  Pointer: 0x1e18218
I 2015-03-29 13:23:35.288  Info line: Hello World!
W 2015-03-29 13:23:35.288      Warning: 0
W 2015-03-29 13:23:35.288      Warning: 1
W 2015-03-29 13:23:35.288      Warning: 2
W 2015-03-29 13:23:35.288      Warning: 3
E 2015-03-29 13:23:35.288  Error: 3.140000
```

Platforms
---------
The library currently works only on Linux. Windows and BSD are on the
roadmap. I don't own any Apple computers, so OS X won't happen unless
someone else sends me a patch or buys me a machine.

Building
--------
To build the library, clone the git repository and run make.

To build a program against the library, given the variable ASYNCLOG
pointing to the root source directory, use:

```bash
g++ myprogram.cpp -I$(ASYNCLOG)/asynclog/include -L$(ASYNCLOG)/asynclog/lib -lasynclog
```



Benchmarks
==========


Libraries
---------
Apart from reckless, the libraries or logging techniques that are benchmarked
here are:
* fprintf. This uses the standard stdio string formatting and calls fflush()
  after each call.
* std::fstream. This uses an fstream object and streams `std::flush` after each
  log line.
* [spdlog](https://github.com/gabime/spdlog/). This is closest to reckless in
  its design goals. It tries to be very fast, and offers an asynchronous mode.
  See notes below on how it is used in the benchmark.
* [pantheios](http://www.pantheios.org/) is often recommended as a good
  library with high performance. The author describes it more as a
  “logging API” that lets you plug in different libraries at the back
  end. However, it offers some stock back ends, and I suspect that most people
  use these built-in facilities. I use the “simple” frontend and the “file”
  backend.
* No operation. This means that the log call is ignored and no code is
  generated apart from the timing code. For the scenarios that measure
  individual call timings, this gives us an idea of how much overhead is added
  by the measurement itself. For the scenarios that measure total execution
  time, this lets us compare the execution time to what it would be if no
  logging occurred at all.
  
How to read the benchmarks
--------------------------
When you make claims about performance, people expect that they are backed up
with measurements. But it is important to realize that your use case and
constraints can have a big impact on how performance is measured and,
consequently, what results you get. For example, reckless allows you to pass raw
pointers to the background thread even though they may no longer be valid by
the time they are written to disk. This can improve performance but can lead
to strange crashes if you make mistakes. Other logging libraries usually
prepare the log string at the call site, which avoids the risk of using a
dangling pointer but increases call latency. This is an intentional design
tradeoff, and no two alternatives can be compared without considering what
those tradeoffs are.

For reckless the following assumptions were made, and the benchmarks were
developed according to those assumptions:
* It is important to minimize the risk of losing log messages in the event of
  a crash.
* It is OK to trade some precision in floating-point output for added
  performance.
* We are concerned with the impact of actual logging, not of logging calls
  that are filtered out at runtime (say, debug messages that are disabled via
  some compile-time or run-time switch). We expect to produce many log
  messages in production environments, not just in debug mode. It is too late
  to enable log messages post-mortem.
* We care enough about performance that we will accept the risk for
  dangling pointer references. Note that you should be OK as long as you never
  pass a raw pointer to dynamically allocated or stack-allocated memory.
  Pointers to global objects are usually fine, and so are string literals. For
  dynamically allocated strings you should use e.g. `std::string`.

Another factor that I care about is that latency should be kept at a stable,
insignificant level. If the write buffer is very large then the user may
experience sudden hangs as the logger suddenly needs to flush large amounts of
data to disk. The output-buffer size in reckless is configurable and could be
set very large if desired, but the default size is 8 KiB. This is large enough
to fit a modern disk sector (4 KiB) while still leaving some room to grow.
  
In the [benchmark made by the spdlog
author](https://github.com/gabime/spdlog/blob/06e0b0387a27a6e77005adac87f235e744caeb87/bench/spdlog-async.cpp),
the asynchronous queue at least 50 MiB in size, in order to fit all log
messages without having to flush. The measurement does not include log setup
and teardown.  In other words, it only measures time for pushing log entries
on the asynchronous queue and not the time for flushing all those messages to
disk. *This is fine*, if:

* You can afford a large memory enough buffer that it will never run out of
  space (but keep in mind that if you make it too large, disk swapping can
  occur and nullify your gains).
* Your process is long-running and you trust that the data will eventually get
  written to disk.
* It is OK to lose a large number of log messages in the event of a crash.

But since the constraints are different in this benchmark, the spdlog buffer
size was set to roughly 8 KiB (128 entries), and the file sink was put in
force-flush mode to ensure that messages are written early instead of being
kept around indefinitely in the stdio buffer.  This corresponds to the
behavior of reckless, which flushes whenever it can and defaults to an 8 KiB
buffer size.

The [Pantheios performance article](http://www.pantheios.org/performance.html)
shows performance both when logging is turned on and off. It claims that
“Pantheios is 10-100+ times more efficient than any of the leading diagnostic
logging libraries currently in popular use.” I’m not sure that this can be
concluded from the charts at all, but in the event that it can, it applies
*only when logging is turned off*. For this benchmark I have only benchmarked
Pantheios for the case when logging is turned *on*. Again, the Pantheios
developers have clearly set different goals with their benchmarks than I do
here.

On a similar notion, for stdio and fstream the file buffer is flushed after
each write. Note that in all libraries tested, flushing means sending the data
to the OS kernel, not actually writing to disk. Sending it to the kernel is
enough to ensure that the data will survive a software crash, but not an OS
crash or power loss.

The specifications of the test machine are as follows:
* Intel i7-3770K CPU at 3.5 Ghz with 4 CPU cores. Hyper-threading and
  SpeedStep is turned off.
* Western Digital Black WD7501AALS 750GB mechanical disk.
* 8 GiB RAM.
* gcc 4.8.4.
* Linux kernel 3.14.14.

For the scenarios that measure individual call timings, measurements were made
according to the article “[How to Benchmark Code Execution Times on Intel IA-32
and IA-64 Instruction Set
Architectures](http://www.intel.com/content/www/us/en/intelligent-systems/embedded-systems-training/ia-32-ia-64-benchmark-code-execution-paper.html)”
by Gabriele Paoloni. To avoid problems with unsynchronized time-stamp counters
across CPU cores, each measured thread is forced to run on a specific CPU core
by setting the thread affinity.

For tests that only measure total execution time, `std::chrono::steady_clock`
is used.
  
Periodic calls
--------------
![Periodic calls performance
chart](images/performance_periodic_calls_all.png)

[benchmarks/periodic_calls.cpp](../benchmarks/periodic_calls.cpp)

In this scenario we simulate a single-threaded program that makes regular
calls to the log, but not so often that the buffer is anywhere near filling
up. Other than logging, the disk is not busy doing anything. It is the most
forgiving scenario, but probably also very common in interactive applications.
The two asynchronous libraries predictably perform much better than the
synchronous ones, since they do not have to wait for the I/O calls. They also
have a more stable execution time. If we zoom in on the asynchronous
libraries we also get a better idea of the measurement overhead.

![Periodic calls performance chart for asynchronous
libraries](images/performance_periodic_calls_asynchronous.png)

The average call latencies are as follows:

Library   | Ticks
----------|------
nop       | 111
reckless  | 812
spdlog    | 1807
fstream   | 22703
stdio     | 26124
pantheios | 37683

Call burst
----------
![Call burst performance chart](images/performance_call_burst_1.png)

[benchmarks/call_burst.cpp](../benchmarks/call_burst.cpp)

This scenario stresses the log by generating messages as fast as it can,
filling up the buffer. The plot is zoomed in so we can see curves for all the
libraries. At first sight the asynchronous alternatives appear to perform
well, but there are now spikes in the call latency that appear when the buffer
fills up. These spikes in fact go as far as 750 000 ticks for reckless, and 25
000 000 ticks for spdlog. By applying a moving average we get a better idea of
the overall performance:

![Call burst performance chart with moving
average](images/performance_call_burst_1_ma.png)

Spdlog performs well until the buffer fills up, but then it stalls waiting for
the buffer to be emptied to disk. After that it stabilizes, but comes out as
the slowest performer, followed by pantheios. It can now be seen that reckless
is still on average the best performer, but it is clear that it does stall
each time the buffer fills up.

It should be noted that while reckless has been profiled and optimized to
handle this situation as gracefully as it can, it is far from an ideal
situation. In general, if your buffer fills up due to a sporadic burst of data
then you should consider enlarging the buffer.

These performance measurements were last made on 2015-21-29. The results may
have changed since then.

Methodology
===========
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
developed according to these assumptions:
* It is important to minimize the risk of losing log messages in the event of
  a crash.
* It is OK to trade some precision in floating-point output for added
  performance.
* We are concerned with the impact of actual logging, not of logging calls
  that are filtered out at runtime (say, debug messages that are disabled via
  some compile-time or run-time switch). We expect to produce many log
  messages in production environments, not just in debug mode. Enabling log
  messages post-mortem is too late.
* We care enough about performance that we will accept the risk for
  dangling pointer references. Note that you should be OK as long as you never
  pass a raw pointer to dynamically allocated or stack-allocated memory.
  Pointers to global objects are usually fine, and so are string literals. For
  dynamically allocated strings you should use e.g. `std::string`.

Another factor that is important in my opinion is that latency should be kept
at a predictable and stable level. If the write buffer is very large then the
user may experience sudden hangs as the logger suddenly needs to flush large
amounts of data to disk. The output-buffer size in reckless is configurable
and could be set very large if desired, but the default size is 8 KiB. This is
large enough to fit a modern disk sector (4 KiB) while still leaving
some room to grow.
  
In the [benchmark made by the spdlog
author](https://github.com/gabime/spdlog/blob/06e0b0387a27a6e77005adac87f235e744caeb87/bench/spdlog-async.cpp),
the asynchronous queue is over 50 MiB in size, in order to fit all log messages
without having to flush. The measurement does not include log setup and
teardown.  In other words, it only measures time for pushing log entries on the
asynchronous queue and not the time for flushing all those messages to disk.
*This is fine if*:

* You can afford a large memory enough buffer that it will never run out of
  space (but keep in mind that if you make it too large, disk swapping can
  occur and nullify your gains).
* Your process is long-running and you trust that the data will eventually get
  written to disk.
* It is OK to lose a large number of log messages in the event of a crash.

But since the constraints are different in this benchmark,
the spdlog asynchronous buffer size was set to roughly 8 KiB (128 entries),
and the sink was put in force-flush mode to ensure that messages are written
early and are not kept around in the stdio buffer. This corresponds to the
behavior of reckless, which flushes whenever it can and defaults to an 8 KiB
buffer size.

The [Pantheios performance article](http://www.pantheios.org/performance.html)
shows performance both when logging is turned on and off. It claims that
“Pantheios is 10-100+ times more efficient than any of the leading diagnostic
logging libraries currently in popular use.” I’m not sure that this can be
concluded from the charts at all, but in the event that it can, it applies when
only *logging is turned off*. For this benchmark, I have only benchmarked
Pantheios for the case when logging is turned on. Again, the Pantheios
developers have clearly set different goals with their benchmarks than I do
here.

On a similar notion, for stdio and fstream the file buffer is flushed after
each write. Note that by flushing I mean sending the data to the OS kernel, not
actually writing to disk. Sending it to the kernel is enough to ensure that the
data will survive a software crash, but not an OS crash or power loss.



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
2. Call to Boost.Lockless (NB: this bundled with the library, not an
   external dependency) to register the write request on a shared
   lockless queue.

This removes or hides several costs:

* There is no transition to the kernel at the call site. This is an
  easily overlooked but important cost, not only because the transition
  costs time, but because it pollutes the CPU cache. In other words,
  avoiding this *makes your non-logging code run faster* than if you
  were using a library that has to enter the kernel to perform its
  work.
* No locks need to be taken for synchronization between threads (unless
  the queue fills up; see the performance sections for more information
  about the implications of this).
* It doesn't have to wait for the I/O operation to complete.
* If there are bursts of log calls, multiple items on the queue can be
  combined into a single write.

What's the catch?
-----------------


Performance
-----------

Why is it so fast?
------------------

Only the
bare minimum of work is performed, which is to save the arguments on a
lockless queue and return. The remainder of the work, i.e. string
formatting and disk i/o is performed on a separate thread. Apart from
not having to wait for the actual I/O operation (or more likely, copying
the data to the OS cache) to complete, this has three advantages:

1. It avoids costly calls to the kernel. Even an ordinary queue with
   mutex locks are expensive because of the cost associated with
   switching CPU context to privileged mode.
2. It doesn't slow down the rest of your code. Calling the OS kernel is
   likely to severely pollute your CPU cache, evicting the data you were
   working on to RAM. In other words, *it makes your non-logging code
   run faster* than if you were using a logging library that has to
   enter the kernel to perform its work.
3. 

The idea is to
just keep the logging arguments on a queue

What's the catch?
-----------------



------------------

Introduction
------------
![Performance chart](doc/images/performance_periodic_calls.png)

asynclog is an extremely low-latency, lightweight logging library. It
was created because I wanted to perform extensive diagnostic logging
without worrying about performance. Many [other logging
libraries](http://www.pantheios.org/performance.html) boast the ability
to throw log messages away very quickly; asynclog boasts the ability to
keep everything without worrying about performance impact. Filtering can
wait until you want to read the log, or need to clean up disk space.

By low latency I mean that the time from when you call the library and
its return is as low as I could possibly make it. 

basic_log
=========
The base class for all logs is `basic_log`. It provides common functionality
but does not provide any public functions for writing to the log.
```c++
class basic_log {
public:
    basic_log();
    basic_log(writer* pwriter, 
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);
    virtual ~basic_log();
    
    basic_log(basic_log const&) = delete;
    basic_log& operator=(basic_log const&) = delete;

    virtual void open(writer* pwriter, 
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);
    virtual void close();

    bool is_open();
    void panic_flush();

protected:
    template <class Formatter, typename... Args>
    void write(Args&&... args);
};
```

Member functions
----------------
<table>
<tr><td><code>(constructor)</code></td><td>Construct a log.</td></tr>
<tr><td><code>(destructor)</code></td><td>Destruct the log. It will be closed
if open.
</td></tr>
<tr><td><code>open</code></td><td>Open the log. This allocates the necessary buffers,
associates the log with a writer, and starts up the writer thread.</td></tr>
<tr><td><code>close</code></td><td>Close the log. This flushes all queued log data in a
controlled manner, then shuts down the background thread and disassociates the
writer.</td></tr>
<tr><td><code>panic_flush</code></td><td>Perform the minimum required work to
write everything that has been sent to the log up to now. This is meant to be
called when a fatal program error (i.e. crash) has occurred, and it is expected
that the process will be terminated after the call. The log object is left in a
"panic" state that prevents any cleanup in the destructor. Any thread that
tries to write to the log after this will sleep indefinitely.</td></tr>
<tr><td><code>write</code></td><td>Store <code>args</code> on the
asynchronous queue and invoke
<code>Formatter::format(output_buffer*, Args...)</code>
from the background thread.
</table>

Arguments
---------
<table>
<tr><td><code>output_buffer_max_capacity</code></td><td></td></tr>
</table>

policy_log
==========

severity_log
------------

Custom writers

Rolling your own logger
-----------------------

Performance
-----------



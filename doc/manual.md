**Table of Contents**

- [basic_log](#)
	- [Member functions](#)
	- [Arguments](#)
- [policy_log](#)
	- [severity_log](#)
	- [Rolling your own logger](#)
	- [Performance](#)

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
<tr><td><code>(constructor)</code></td><td>Construct a log and optionally open
it if a writer is provided.</td></tr>
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
from the background thread. This is meant to be called from derived classes.
</table>

Arguments
---------
<table>
<tr><td><code>pwriter</code></td><td>Pointer to a writer to use for writing
formatted log data to disk or other targets.
<tr><td><code>output_buffer_max_capacity</code></td><td>Maximum number of bytes
that may be allocated for the final, formatted output buffer. If 0 is
specified, the CPU page size is used (on Intel this is commonly 4
KiB).</td></tr>
<tr><td><code>shared_input_queue_size</code></td><td>Maximum number of log
entries in the queue shared between application threads and the background
writer thread.  If 0 is specified, the library picks a number that fits in a
single CPU memory page. The shared queue stores references to the thread-local
log buffers and the current write position in them.</td></tr>
<tr><td><code>thread_input_buffer_size</code></td><td>Maximum number of bytes
that may be pushed on the thread-local log buffer. This stores the actual
arguments passed to <code>write()</code> and a function pointer, for each log
entry.</td></tr>
</table>

policy_log
==========
`policy_log` supports `printf`-like formatting, configurable header
fields, and scope-based indenting.

```c++
template <class IndentPolicy = no_indent, char FieldSeparator = ' ', class... HeaderFields>
class policy_log : public basic_log {
public:
    policy_log();
    policy_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);

    template <typename... Args>
    void write(char const* fmt, Args&&... args);
};
```

Member functions
---------
<table>
<tr><td><code>write</code></td><td>Write a formatted line to the log.</td></tr>
</table>

Arguments
---------
See [basic_log](#) for constructor arguments.
<table>
<tr><td><code>IndentPolicy</code></td>
<td>May be one of:
<ul>
<li><code>no_indent</code> to disable scope-based indentation.</li>
<li><code>indent&lt;Multiplier, Character&gt;</code> where
<code>Character</code> is the indentation character to use, and
<code>Multiplier</code> is how many indentation characters to output for each
indentation level. For example, <code>indent&lt;4, ' '&gt;</code> or
<code>indent&lt;1, '\t'&gt;</code>.</li>
</ul>
<tr><td><code>FieldSeparator</code></td><td>Character to use for separating
log fields.</td></tr>
<tr><td><code>HeaderFields</code></td><td>One or more fields to use for
prefixing each log line. The only field currently available is
<code>timestamp_field</code> which will output the time in ISO 8601 compliant
time format. Other fields can be be implemented by the client; see the
implementation of <code>timestamp_field</code> for more information.</td></tr>
<tr><td><code>fmt</code></td><td>Format string. The conversion specifiers are
parsed differently depending on the type of each converted argument, but are
roughly equivalent to <code>printf</code> for native types. There is no need
to end the string with a newline as <code>write</code> always writes each
string on a separate line. This behavior is mostly to avoid spending CPU
cycles on splitting the output to insert the header fields at
the beginning of each new line, but also to make logging simpler for the
caller.</td></tr>
<tr><td><code>args</code></td><td>Data to print. For each argument,
<code>format(output_buffer*, char const* fmt, T&&)</code> is invoked to write
<code>T</code> as formatted data to the output buffer. Argument-dependent
lookup applies, so the client may declare <code>format</code> in the same
namespace as <code>T</code>.</td></tr>
</table>

When `IndentPolicy` is set to `indent`, each instance of `scoped_indent` will
increase the indentation level by one during its life time. Note that the
indentation level is necessarily a thread-local property, so if you have
multiple writer threads then your log may end up with interleaved lines with
dififerent indentation levels. One way of dealing with this is to have a field
containing the thread ID, and filtering the log by thread.

severity_log
============
The severity log is identical to `policy_log` except that it provides four
write functions instead of one.

```c++
template <class IndentPolicy, char FieldSeparator, class... HeaderFields>
class severity_log : public basic_log {
public:
    severity_log()
    {
    }

    severity_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);

    template <typename... Args>
    void debug(char const* fmt, Args&&... args);
    
    template <typename... Args>
    void info(char const* fmt, Args&&... args);
    
    template <typename... Args>
    void warn(char const* fmt, Args&&... args);
    
    template <typename... Args>
    void error(char const* fmt, Args&&... args);
};
```

Each of these signifies a different severity level. In my experience,
severity levels in log files easily become a point of contention, so if
you wish to use them you may want to roll your own class based on this,
and make sure that you have well-defined meanings of each log level.
Below are definitions that I propose for this class.

| Severity | Proposed meaning |
|:---------|:-----------------|
| `debug`  | Log lines used while debugging a specific issue. |
| `info`   | Information about successful operations, operations that are about to start, or about the current state of the program. |
| `warn`   | Indicates an issue that may potentially lead to an error later on. |
| `error`  | Indicates an issue that prevents the program from working as intended. |
------------------------------

The severity level can be placed on the log line by including `severity_field`
as one of the header fields. This will output `D`, `I`, `W` or `E` to indicate
which of the four functions was called.

Custom writers
==============
To customize how reckless logs data, you implement the `writer`
interface.
```c++
class writer {
public:
    enum Result
    {
        SUCCESS,
        ERROR_TRY_LATER,
        ERROR_GIVE_UP
    };
    virtual ~writer() = 0;
    virtual Result write(void const* pbuffer, std::size_t count) = 0;
};
```

The `write` function should attempt to write `count` bytes from the
buffer pointed to by `pbuffer`. If it returns `SUCCESS` then the log
will consider the data to be persisted and will discard it from memory.

The other two return values are not yet honored by the log at the time
of this writing, but their meaning will be as follows. If
`ERROR_GIVE_UP` is returned then the background thread will stop
writing. The log will continue to operate but all data will be silently
ignored. If `ERROR_TRY_LATER` is returned then the background thread
will keep trying as new data is received. This is intended to be used
for temporary situations, for example shortage of disk space or memory.
If any buffers fill up then log functions will not block but discard
data to prevent the program from freezing. The implementation may wish
to output some kind of diagnostic message about this when the writer is
again able to write data.


Custom string formatting for your own data types
================================================

Rolling your own logger
=======================
example: ucs-2 logger
example: root / hierarchical logger

Handling crashes
================

Performance
===========



Table of Contents
=================

- [basic_log](#basic_log)
- [policy_log](#policy_log)
- [severity_log](#severity_log)
- [Custom writers](#custom-writers)
- [file_writer](#file_writer)
- [stdout_writer and stderr_writer](#stdout_writer-and-stderr_writer)
- [Custom string formatting](#custom-string-formatting)
- [output_buffer](#output_buffer)
- [Custom fields in policy_log](#custom-fields-in-policy_log)
- [Rolling your own logger](#rolling-your-own-logger)
- [A note on move semantics](#a-note-on-move-semantics)
- [Handling crashes](#handling-crashes)
- [Limited floating-point accuracy](#limited-floating-point-accuracy)

basic_log
=========
The base class for all logs is `basic_log`. It provides common functionality
but does not provide any public functions for writing to the log.
```c++
// #include <reckless/basic_log.hpp>

using format_error_callback_t = std::function<
    void (output_buffer*, std::exception_ptr const&, std::type_info const&)
>;

using writer_error_callback_t = std::function<
    void (output_buffer* pbuffer, std::error_code ec, unsigned lost_record_count)
>;

class basic_log {
public:
    basic_log();
    basic_log(writer* pwriter);
    basic_log(writer* pwriter,
        std::size_t input_buffer_capacity,
        std::size_t output_buffer_capacity);
    virtual ~basic_log();

    basic_log(basic_log const&) = delete;
    basic_log& operator=(basic_log const&) = delete;

    void open(writer* pwriter);
    void open(writer* pwriter,
        std::size_t input_buffer_capacity,
        std::size_t output_buffer_capacity);

    virtual void close(std::error_code& ec) noexcept;
    virtual void close();

    virtual void flush(std::error_code& ec);
    virtual void flush();

    void format_error_callback();
    void format_error_callback(format_error_callback_t format_error_callback);
    void writer_error_callback();
    void writer_error_callback(writer_error_callback_t writer_error_callback);

    error_policy temporary_error_policy() const;
    void temporary_error_policy(error_policy ep);
    error_policy permanent_error_policy() const
    void permanent_error_policy(error_policy ep);

    void start_panic_flush();
    void await_panic_flush();
    bool await_panic_flush(unsigned int miliseconds);

    std::thread& worker_thread();

    unsigned input_buffer_full_count() const
    unsigned input_buffer_high_watermark() const
    unsigned output_buffer_full_count() const;
    std::size_t output_buffer_high_watermark() const;

protected:
    template <class Formatter, typename... Args>
    void write(Args&&... args);
};
```

Member functions
----------------
<table>
<tr><td><code>(constructor)</code></td>
<td>Construct a log and optionally open it if a writer is provided.</td></tr>

<tr><td><code>(destructor)</code></td> <td>Destruct the log. It will be closed
if open.</td></tr>

<tr><td><code>open</code></td>
<td>Open the log. This allocates the necessary buffers, associates the log with
a writer, and starts up the writer thread.</td></tr>

<tr><td><code>close</code></td>
<td>Close the log. This flushes all queued log data in a controlled manner,
then shuts down the background thread and disassociates the writer. Writing to
or closing a log that is not open leads to undefined behavior.</td></tr>

<tr><td><code>flush</code></td>
<td><p>Wake up the output worker thread and then block until it has formatted
and written all log messages up until the most recent one.</p>
This function can be used to ensure that all log messages have reached the
writer before continuing execution of the program, or possibly to check whether
the writer is still successfully writing data. It is not meant to be a low-
latency call and involves creating a temporary thread synchronization
object.</td></tr>

<tr><td><code>format_error_callback</code></td>
<td>Set a function that will be called if an exception is caught while
performing formatting of a log entry in the background thread.</td></tr>

<tr><td><code>writer_error_callback</code></td>
<td>Set a function that will be called when the writer returns to a working
state after log messages have been lost due to failed writes. It will only be
called when the error policy is <code>notify_on_recovery</code>. Passing no
argument will disable error recovery notifications.</td></tr>

<tr><td><code>temporary_error_policy</code></td>
<td>Set or get the policy how to behave when temporary errors occur. The
classification of errors as temporary is up to the <code>writer</code>
object.</td></tr>

<tr><td><code>permanent_error_policy</code></td>
<td>Set or get the policy for how to behave when permanent errors occur. The
classification of errors as permanent is up to the <code>writer</code>
object.</td></tr>

<tr><td><code>panic_flush</code></td>
<td>Perform the minimum required work to write everything that has been sent to
the log up to now. This is meant to be called when a fatal program error (i.e.
crash) has occurred, and it is expected that the process will be terminated
after the call. The log object is left in a "panic" state that prevents any
cleanup in the destructor. Any thread that tries to write to the log after this
will be suspended.</td></tr>

<tr><td><code>worker_thread</code></td>
<td>Provide access to the internal worker-thread object. The intent is to allow
platform-specific manipulation of the thread, such as setting priority or
affinity. Use good judgement when accessing this object, as any destructive
action such as moving or detaching it can cause reckless to
malfunction.</td></tr>


<tr><td><code>input_buffer_full_count</code></td>
<td>Return number of times that the input buffer has become full and caused the
caller thread to block. If this number increases often then either log messages
are being written at a rate higher than the capacity of the writer (unlikely) or
the input-buffer size needs to be increased to properly cushion sudden spikes in
the output rate.
</td>
</tr>

<tr><td><code>input_buffer_high_watermark</code></td>
<td>Return the highest number of bytes ever in use in the input buffer. While <code>input_buffer_full_count</code> can be used to determine if the buffer needs to grow, this can be used to determine how much the buffer can be shrunk.<td>
</tr>

<tr><td><code>output_buffer_full_count</code></td>
<td>Return number of times that the output buffer became full before all
available entries in the input buffer were processed. Ideally all available
entries can be processed without filling up the output buffer. That way writes
are performed only while the input buffer has the most available space, and much
less time will be spent in I/O. There is a causal relation between a full output
buffer and a full input buffer, as an output buffer that is too small may reduce
throughput and increase the needed input-buffer space. As such it may be a good
idea to balance the output-buffer size before adjusting the input buffer.</td>
</tr>

<tr><td><code>output_buffer_high_watermark</code></td>
<td>Return the highest number of bytes ever in use in the output buffer.</td>
</tr>

<tr><td><code>write</code></td>
<td>Store <code>args</code> on the asynchronous queue and invoke the static
function <code>Formatter::format(output_buffer*, Args...)</code> from the
background thread. This is meant to be called from derived classes. Calling it
when the log is in a closed state leads to undefined behavior.</td></tr>
</table>

Arguments
---------
<table>
<tr><td><code>pwriter</code></td>
<td>Pointer to a writer to use for writing formatted log data to disk or other
targets.</td></tr>

<tr><td><code>input_buffer_capacity</code></td>
<td>Capacity of the input buffer. This should be large enough to fit any burst
of log messages that your application might produce over a short period of time.
A log entry stores all arguments passed to <code>basic_log::write</code> packed
as close as possible without violating alignment requirements of each type, and
is padded so that the size is rounded up to the nearest multiple of the
cache-line size (64 bytes on x86 architectures). On Windows, the buffer size is
always rounded up to the nearest multiple of 64 KiB to meet the requirements for
creating a magic ring buffer. On Linux the granularity is 4 KiB. If this
argument is not provided or it has the value 0 then reckless uses the default of
64 KiB, which is the minimum size possible on Windows.</td></tr>

<tr><td><code>output_buffer_capacity</code></td>
<td>Capacity of the final formatted output buffer. If not provided or set to 0,
a heuristic based on the input buffer size is used.</td></tr>

<tr><td><code>shared_input_queue_size</code></td>
<td>Maximum number of log entries in the queue shared between application
threads and the background writer thread.  If 0 is specified, the library picks
a number that fits in a single CPU memory page. The shared queue stores
references to the thread-local log buffers and the current write position in
them.</td></tr>

<tr><td><code>thread_input_buffer_size</code></td>
<td>Maximum number of bytes that may be pushed on the thread-local log buffer.
This stores the actual arguments passed to <code>write()</code> and a function
pointer, for each log entry.</td></tr>

<tr><td><code>ec</code></td>
<td><code>std::error_code</code> instance to use for reporting errors from the
writer. If no error occurs, <code>ec.clear()</code> is called. If
<code>ec</code> is not given, then the error will be thrown as a
<code>writer_error</code> exception instead.</td></tr>

<tr><td><code>format_error_callback</code></td>
<td><p>A function object with the signature <code>void (output_buffer*,
std::exception_ptr const&, std::type_info const&)</code> that is called when an
exception occurs during formatting of a log message. The callback must not write
to the <code>basic_log</code> instance, since this can cause a deadlock or a
never-ending loop. However, it may write directly to the output buffer. The
<code>exception_ptr</code> points to the exception that was caught. The
<code>type_info</code> instance provides type information for the arguments that
were passed to the formatting function (i.e. the arguments that were passed to
<code>basic_log::write</code>) wrapped as template arguments for
<code>std::tuple</code>.</p>
Omitting this argument will cause formatting errors to be ignored, and
failed messages will be discarded.</td></tr>

<tr><td><code>writer_error_callback</code></td>
<td>A function object with the signature <code>void (output_buffer*,
std::error_code, unsigned)</code> that is called when the writer returns to a
working state after log messages have been lost due to failed writes. The
callback must not write to the <code>basic_log</code> instance, since this can
cause a deadlock or a never-ending loop. However, it may write directly to the
output buffer. The <code>error_code</code> provides the error code returned by
the writer. The integer argument will be at least 1, and specifies how many log
messages have been discarded because of failed writes since the last successful
write. The callback may not throw any exceptions.</td></tr>

<tr><td><code>ep</code></td>
<td>Indicates policy for handling writer errors as follows:
<table>
<tr><td><code>ignore</code></td>
<td>Ignore errors and discard messages that could not be written.</td></tr>
<tr><td><code>notify_on_recovery</code></td>
<td>Discard messages that could not be written but keep a tally on how many
messages were lost. Once the writer is able to successfully write again,
notify the <code>writer_error_callback</code> about how many messages were
lost.</td></tr>
<tr><td><code>block</code></td>
<td>Leave messages in the queue. Once the queue fills up, any attempts to write
to the log will block until the writer succeeds again.</td></tr>
<tr><td><code>fail_immediately</code></td>
<td><p>As soon as a write fails the log instance will enter an error state, and
all attempts to write to the log will throw <code>writer_error</code>
indicating the <code>error_code</code> that occurred. Note that because of the
asynchronous one-way nature of the log, a failure to write message A will not
be seen by the log call that puts message A on the queue. Rather, it is when
message A reaches the point where it will be written by the background thread
that the error will be known. A message B which is put on the queue later may
then throw an exception due to the error that occured when A was sent to the
writer.</p>

<p>In other words, a writer_error exception cannot indicate the exact point at
which the error occurred. Instead it should be seen as a mechanism for aborting
a subroutine as soon as possible when it becomes known that messages are not
reaching the log file properly.</p>

If the writer begins successfully writing data again, then the internal error
state will be cleared and it will be possible to put new messages on the queue
again.</td></tr>
</table>

<tr><td><code>Formatter</code></td><td>A type that provides the member function
<code>static void format(output_buffer*, Args...)</code>. <code>Args</code>
should be compatible with the arguments that you intend to pass to
<code>write</code>. The task of the <code>Formatter</code> is to write
formatted data to the provided <code>output_buffer</code> based on the values
of <code>Args</code>.</td></tr>
</table>

policy_log
==========
`policy_log` supports `printf`-like typesafe formatting, configurable header
fields, and scope-based indenting.

```c++
// #include <reckless/policy_log.hpp>

template <class IndentPolicy = no_indent, char FieldSeparator = ' ', class... HeaderFields>
class policy_log : public basic_log {
public:
    policy_log();
    policy_log(writer* pwriter);
    policy_log(writer* pwriter,
        std::size_t input_buffer_capacity,
        std::size_t output_buffer_capacity);

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
time format. Other fields can be be implemented by the client; see <a href
="#custom-fields-in-policy_log">Custom fields in policy_log</a> for more
information.</td></tr>
<tr><td><code>fmt</code></td><td>Format string. The
conversion specifiers are parsed differently depending on the type of each
converted argument, but are similar to <code>printf</code> for native
types. One exception is that <code>%d</code> is used for all integer types.
Other integer format specifiers like <code>%lu</code> or <code>PRIu64</code> are not supported.
There is no need to end the string with a newline as <code>write</code>
always writes each string on a separate line. This behavior is mostly to avoid
spending CPU cycles on splitting the output to insert the header fields at the
beginning of each new line, but also to make logging simpler for the
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
// #include <reckless/severity_log.hpp>

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
To customize where log data ends up, you implement the `writer` interface.
```c++
// #include <reckless/writer.hpp>

namespace reckless
{
    class writer {
    public:
        enum errc
        {
            temporary_failure = 1,
            permanent_failure = 2
        };
        static std::error_category const& error_category();

        virtual ~writer() = 0;
        virtual std::size_t write(void const* pbuffer, std::size_t count,
            std::error_code& ec) noexcept = 0;
    };

    std::error_condition make_error_condition(writer::errc);
    std::error_code make_error_code(writer::errc);
}   // namespace reckless

namespace std
{
    template <>
    struct is_error_condition_enum<reckless::writer::errc> : public true_type {};
}
```

The `write` function should attempt to write `count` bytes from the
buffer pointed to by `pbuffer`. It should call `ec.clear()` and return `count`
to indicate a successful write. The log will then consider the data to be
persisted and will discard it from memory.

If an error occurs then an error code should be stored in `ec` and the number of
successfully written bytes should be returned. The log will discard only the
successfully written bytes from the log.

Error codes returned by the writer must have an `error_category` that implements
`equivalent` such that either `ec == writer::temporary_failure` or `ec ==
writer::permanent_failure` is true. If neither is true then reckless will assume
that it is a permanent error. By permanent we mean that it is assumed that the
writer will never recover from the error condition.

Implementing an error category does not take a lot of code and is fairly simple.
However, at the time of writing this, documentation on error categories is
pretty scarce. You may wish to refer to the source code for `fd_writer` for
an example of how to correctly implement an error category for your error codes.

file_writer
===========
`file_writer` is a simple implementation of the `writer` interface that
continuously writes to a single file. The file path is passed as an
argument to the constructor. If it already exists then the writer will
append to the existing file.

```c++
// #include <reckless/file_writer.hpp>

class file_writer : public writer {
public:
    file_writer(char const* path);
#if defined(_WIN32)
    file_writer(wchar_t const* path);
#endif

    ~file_writer();
    std::size_t write(void const* pbuffer, std::size_t count,
        std::error_code& ec) noexcept override;
};
```

On Linux, the writer classifies following error codes as temporary errors:
`ENOSPC` (disk full), `ENOBUFS` (out of memory),
`EDQUOT` (user quota reached), `EIO`
(low-level I/O error). On
Windows, the following errors are classified as temporary errors:
- `ERROR_BUSY`
- `ERROR_DISK_FULL`
- `ERROR_HANDLE_DISK_FULL`
- `ERROR_LOCK_VIOLATION`
- `ERROR_NOT_ENOUGH_MEMORY`
- `ERROR_NOT_ENOUGH_QUOTA`
- `ERROR_NOT_READY`
- `ERROR_OPERATION_ABORTED`
- `ERROR_OUTOFMEMORY`
- `ERROR_READ_FAULT`
- `ERROR_RETRY`
- `ERROR_SHARING_VIOLATION`
- `ERROR_WRITE_FAULT`
- `ERROR_WRITE_PROTECT`

All other errors are classified as permanent.

stdout_writer and stderr_writer
===============================
`stdout_writer` and `stderr_writer` write to the respective standard streams.

```c++
// #include <reckless/stdout_writer.hpp>

class stdout_writer : public writer {
public:
    stdout_writer();
    std::size_t write(void const* pbuffer, std::size_t count,
        std::error_code& ec) noexcept override;
};

class stderr_writer : public writer {
public:
    stderr_writer();
    std::size_t write(void const* pbuffer, std::size_t count,
        std::error_code& ec) noexcept override;
};
```

The error categorization is identical to that of `file_writer`.

Custom string formatting
================================================
Both `policy_log` and `severity_log` make use of the `template_formatter`
class, which provides `printf`-like formatting of strings. However, it also
allows the caller to define formatting for user-defined types. For every
argument of type `T&&` passed to the log function,

```c++
char const* format(output_buffer*, char const* fmt, T&&)
```

is called to write a formatted version of it to the output buffer. The `fmt`
parameter points to the corresponding format specification in the format
string, after the `%` escape character. The function should consume one or
more characters from fmt to use as a format specification, write formatted
output to the `output_buffer` pointer, and return a pointer to the first
character in the format string that is not considered part of the format
specification. If the format specification is ill-formed then `nullptr` should
be returned.

For example, in the format string `"Hello %s, how are you"`, `fmt` would point
to `s`. The function would return a pointer to the blank space after `s`.

Argument-dependent lookup applies for the call, so you can declare `format` in
the same namespace as `T`. The library provides a `format` implementation for
all the native types, so you may piggy-back on that for your own implementation.

output_buffer
=============
The `output_buffer` class accumulates formatted data and flushes it to disk
when appropriate. The task of the client-provided `format` function is to
write data to the `output_buffer`.

```c++
// #include <reckless/output_buffer.hpp>

class output_buffer {
public:
    char* reserve(std::size_t size);
    void commit(std::size_t size);
    void write(void const* buf, std::size_t count);
    void write(char const* s);
    void write(char c);
};
```

Member functions
----------------
<table>

<tr><td><code>reserve</code></td><td>Ensure there is enough space in the
buffer and obtain a pointer to the current position.</td></tr>

<tr><td><code>commit</code></td><td>Commit previously reserved bytes to the
buffer.</td></tr>

<tr><td><code>write</code></td><td>Write provided data directly to the buffer.</td></tr>

</table>

The intended usage pattern is to make a pessimistic guess for how much space
will be required in the buffer for the data that is to be written, and reserve
that much space. After data has been written to the buffer, the number of
bytes that were actually used are committed. It is possible to reserve
memory once and commit multiple times, as long as the sum of what you commit
is never larger than what you reserved. Calling `reserve` multiple times will
obtain the same pointer each time until `commit` has been called.

`write` is a shorthand for a combined `reserve` and `commit` call, but has the
opportunity to optimize the operation. In the future it might pass the data
directly to the writer instead of using an intermediate buffer, if you are
writing enough data.

Parameters
----------
<table>
<tr><td><code>size</code></td><td>Number of bytes to reserve or commit.</td></tr>
<tr><td><code>buf</code></td><td>Pointer to buffer</td></tr>
<tr><td><code>count</code></td><td>Number of bytes to write</td></tr>
<tr><td><code>s</code></td><td>Zero-terminated string to write (the zero
terminator is not written).</td></tr>
<tr><td><code>c</code></td><td>Single byte</td></tr>
</table>

Custom fields in policy_log
===========================
If you don't want to build your own logger, the `policy_log`'s `HeaderFields`
parameter provides a simple way to put custom information on each
log line. A field should provide this interface:

```c++
class field {
public:
    field();
    field(field&&);
    bool format(output_buffer* pbuffer);
};
```

When `policy_log::write` is called, each requested field is instantiated using
its default constructor, then moved (or just copied, if no move semantics are
available) to the log queue. In the background thread, `format` is called to
write field data to the output buffer.

For example, the stock `timestamp_field` obtains the current time in the
constructor, and formats it as an ISO 8601 timestamp in `format`. It is
recommended to look at the source code for this if you wish to implement your
own field.

Rolling your own logger
=======================
While `policy_log` and `severity_log` provide good default starting points for
logging, many people may wish to build their own logger interface and only
reuse the underlying asynchronous framework. For example, the
debug/info/warning/error levels in `severity_log` may not be a good fit for
everybody, on Windows you may wish to write data in UCS-2 (Unicode) format
instead, or you might not like the built-in template formatter. For all of
these scenarios, it is easy to implement your own logger.

To implement your own logger, you create a class that derives from `basic_log`,
and optionally build your own formatter class (if you don't want to use
`template_formatter`). For example, the following could be used for writing
simple UCS-2 strings to a log file.

```c++
#include <reckless/basic_log.hpp>
#include <reckless/file_writer.hpp>
#include <string>

class ucs2_log : public reckless::basic_log {
public:
    ucs2_log(reckless::writer* pwriter) : basic_log(pwriter) {}
    void puts(std::wstring const& s)
    {
        basic_log::write<ucs2_formatter>(s);
    }
    void puts(std::wstring&& s)
    {
        basic_log::write<ucs2_formatter>(std::move(s));
    }
private:
    struct ucs2_formatter {
        static void format(reckless::output_buffer* pbuffer, std::wstring const& s)
        {
            pbuffer->write(s.data(), sizeof(wchar_t)*s.size());
        }
    };
};

reckless::file_writer writer("log.txt");
ucs2_log g_log(&writer);

int main()
{
    g_log.puts(L"Hello World!\n");
    return 0;
}

```

For more examples, see the source code for the existing loggers.

A note on move semantics
========================
Whenever it can, reckless tries to move objects rather than copy them. In the
example above, when `puts` is called a temporary `std::wstring` object is
created to hold the string.  Because it is an rvalue, this string is moved
into the asynchronous queue rather than copied. When the formatter is called
from the background thread, all argument values are passed as rvalues because
they will no longer be needed and will be destroyed as soon as the formatter
is done. In other words, the formatter may choose to accept its arguments by
value, as normal references, or as rvalue references. In the example, if `s`
was accepted by value then a new `wstring` object would be created on the
stack and `wstring`'s move constructor would be called. If `s` was accepted as
an lvalue or rvalue reference then no new object would be created. A const
lvalue reference is usually the right choice as this avoids creating any new
objects, unless you need to modify the object.

This also means that the following code is valid (assuming that there exists a
formatter for `unique_ptr<char>`):
```c++
reckless::file_writer writer("log.txt");
reckless::policy_log<> log(&writer);
log.write("%c", std::make_unique<char>('A'));
```

That is, you can send an object pointer through the logging queue and have it
automatically deleted after it was written, without the use of any reference
counting.

Handling crashes
================
As with any log that buffers data before writing, there is a risk that data
will get lost if the program crashes. This is particularly unfortunate if the
point of the log was to backtrack the events that lead to a crash. Reckless
tries to minimize the risk by writing as soon as it becomes aware that data is
available. However, a more reliable way to save the data is to call the
`panic_flush` function in `basic_log` from a crash handler. Installing a crash
handler is somewhat involved and platform-specific, so for convenience some
functionality to do this is provided by the library.

```c++
// #include <reckless/crash_handler.hpp>

void install_crash_handler(std::initializer_list<basic_log*> log);
void uninstall_crash_handler();

class scoped_crash_handler {
public:
    scoped_crash_handler(std::initializer_list<basic_log*> log)
    {
        install_crash_handler(log);
    }
    ~scoped_crash_handler()
    {
        uninstall_crash_handler();
    }
};
```

As you might guess, setting up the crash handler is a matter of calling
```c++
install_crash_handler(&my_log);
```
and before the log goes out of scope, call
```c++
uninstall_crash_handler();
```
You may pass multiple log objects to `install_crash_handler`, and if a crash
occurs, `panic_flush` will be called on each of those objects.

Note that if you already have a crash handler of your own, you should simply
add a call to `panic_flush` there instead of using these convenience
functions.

Limited floating-point accuracy
===============================
You should be aware that `template_formatter`, which is used by `policy_log`
and `severity_log` for formatting text, uses a home-grown algorithm for
converting floating-point values to strings. String conversion of IEEE754
floating-point is generally expected to be performed so that it generates as
few digits as possible, but still yields the exact same floating-point number
when it is converted back from a string. Statistical tests on 100 million
randomly generated values show that this is true for 98.5% of the numbers.
99.4% of the numbers are correctly converted, but of those about 1% include
more digits than necessary.

For the numbers that are not correctly converted, the following table shows the
number significant digits that were correct.

Correct significant digits | Number of samples | Percentage
---------------------------|-------------------|-----------
                        10 | 1                 | 0.000001%
                        11 | 6                 | 0.000006%
                        12 | 100               | 0.0001%
                        13 | 1100              | 0.0011%
                        14 | 11889             | 0.011889%
                        15 | 117953            | 0.117953%
                        16 | 446222            | 0.446222%
                        17 | 11371             | 0.011371%


The actual results will depend largely on the quality of your `pow()`
implementation. I made the choice to implement a custom algorithm because
number to string conversion, and in particular floating-point conversions,
turned out to be a performance bottleneck in my benchmark tests. I feel that
for the majority of cases, absolutely perfect accuracy in logging is not as
important as performance. The new algorithm shows improved overall logging
performance, but I have not yet made any detailed performance analysis of the
conversion function itself. It is possible that this algorithm will change in
the future, for example by using the
[Grisu3](http://florian.loitsch.com/publications/dtoa-pldi2010.pdf) algorithm,
and that a more thorough evaluation of performance will be made. However,
printing floating-point numbers is *hard*. I estimate that over 90% of the
development time of the initial reckless release was spent on the number
formatting alone. So improvements in this area are likely going to take a long
time.

**Table of Contents**

- [basic_log](#)
	- [Member functions](#)
	- [Arguments](#)
- [policy_log](#)
	- [severity_log](#)
	- [Rolling your own logger](#)
	- [Performance](#)

Overview of the library
=======================
<Graph>

basic_log
=========
The base class for all logs is `basic_log`. It provides common functionality
but does not provide any public functions for writing to the log.
```c++
// #include <reckless/basic_log.hpp>

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
asynchronous queue and invoke the static function
<code>Formatter::format(output_buffer*, Args...)</code>
from the background thread. This is meant to be called from derived classes.
</table>

Arguments
---------
<table>
<tr><td><code>pwriter</code></td><td>Pointer to a writer to use for writing
formatted log data to disk or other targets.</td></tr>
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
<tr><td><code>Formatter</code></td><td>A type that provides the function
<code>static void format(output_buffer*, Args...)</code>. <code>Args</code>
should be compatible with the arguments that you intend to pass to
<code>write</code>. The task of the <code>Formatter</code> is to write
formatted data to the provided <code>output_buffer</code> based on the values
of <code>Args</code>.</td></tr>
</table>

policy_log
==========
`policy_log` supports `printf`-like formatting, configurable header
fields, and scope-based indenting.

```c++
// #include <reckless/policy_log.hpp>

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
To customize how reckless logs data, you implement the `writer`
interface.
```c++
// #include <reckless/writer.hpp>

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
    ~file_writer();
    Result write(void const* pbuffer, std::size_t count);
};
```

Custom string formatting
================================================
Both `policy_log` and `severity_log` make use of the `template_formatter`
class, which provides `printf`-like formatting of strings. However, it also
allows the caller to define formatting for user-defined types. For every
argument of type `T&&` passed to the log function,

```c++
format(output_buffer*, char const* fmt, T&&)
```

is called to write a formatted version of it to the output buffer.
argument-dependent lookup applies for the call, so the caller may declare
`format` in the same namespace as `T`. The library provides a `format`
implementation for all the native types, so you may piggy-back on that for
your own implementation.

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

`write` is a shorthand for a combined `reserve` and `commit` call, but does
take any opportunities it can to optimize the operation. If you write a large
enough piece of data, it may be passed directly to the writer from your buffer
instead of being copied to the intermediate buffer.

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
parameter `policy_log` provides a simple way to put custom information on each
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
------------------------
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
99.4% of the numbers are correctly converted, but the additional 1% include
more digits than necessary. 

For the numbers that are not correctly converted, the following table shows the
number significant digits that were correct.

Correct significant digits | Number of samples (percentage)
---------------------------|-------------------------------
                        10 | 1 (1e-06%)
                        11 | 6 (6e-06%)
                        12 | 100 (0.0001%)
                        13 | 1100 (0.0011%)
                        14 | 11889 (0.011889%)
                        15 | 117953 (0.117953%)
                        16 | 446222 (0.446222%)
                        17 | 11371 (0.011371%)


I made the choice to implement a custom algorithm because number to string
conversion, and in particular floating-point conversions, turned out to be a
performance bottleneck in my benchmark tests. The new algorithm shows improved
overall logging performance, but I have not yet made any detailed performance
analysis of the conversion function itself. It is possible that this algorithm
will change in the future, for example by using the
[Grisu3](http://florian.loitsch.com/publications/dtoa-pldi2010.pdf) algorithm,
and that a more thorough evaluation of performance will be made. However,
printing floating-point numbers is *hard*. I estimate that over 90% of the
development time of reckless was spent on the number formatting alone. So
improvements in this area is likely going to take a long time.

Performance
===========



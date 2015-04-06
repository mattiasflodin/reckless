basic_log
---------
The base class for all loggers is `basic_log`. It only forms the common
functionality and cannot be directly instantiated (its destructor is
protected).
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
};
```

Member functions
================
<table>
<tr><td>(constructor)</td><td>Construct a logger.</td></tr>
<tr><td> (destructor)</td><td> Destructs the log.                  </td></tr>
<tr><td> open        </td><td> Open the log.                       </td></tr>
<tr><td> close       </td><td>                                     </td></tr>
<tr><td> is_open     </td><td> Return `true` if the log is opened. </td></tr>
</table>


policy_log
----------

severity_log
------------

Custom writers

Rolling your own logger
-----------------------

Performance
-----------



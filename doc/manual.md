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
| (constructor) | Construct a logger. |
| (destructor)  | Destructs the log. |
| open | Open the log. |
| close | 
| is_open | Return `true` if the log is opened. |


policy_log
----------

severity_log
------------

Custom writers

Rolling your own logger
-----------------------

Performance
-----------



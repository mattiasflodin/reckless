#include <initializer_list>

namespace reckless {
class basic_log;
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
}   // namespace reckless

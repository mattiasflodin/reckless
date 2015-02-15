#include <asynclog.hpp>
#include <memory>

int main()
{
    asynclog::file_writer writer("alog.txt");
    asynclog::policy_log<> log(&writer);
    std::unique_ptr<int> pvalue(new int);
    log.write("Allocated pvalue at %s\n", pvalue.get());
    log.write("Value of uninitialized int is %05.3d\n", *pvalue);
    log.write("Value of uninitialized int is %05.3x\n", *pvalue);
}

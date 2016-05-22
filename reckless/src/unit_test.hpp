/* This file is part of reckless logging
 * Copyright 2015, 2016 Mattias Flodin <git@codepentry.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifdef UNIT_TEST

#include <cstddef>  // size_t
#include <initializer_list>
#include <vector>
#include <string>
#include <iostream> // cout, endl
#include <stdexcept>    // logic_error
#include <sstream>  // ostringstream

namespace unit_test {

extern char const* g_current_testcase;
struct no_context
{
};

template <typename Context>
class test {
public:
    test(char const* name, void (Context::*ptest_function)()) :
        name_(name),
        ptest_function_(ptest_function)
    {
    }
    void operator()(Context& ctx)
    {
        (ctx.*ptest_function_)();
    }

    char const* name() const
    {
        return name_;
    }

private:
    char const* name_;
    void (Context::*ptest_function_)();
};

template <>
class test<no_context> {
public:
    test(char const* name, void (*ptest_function)()) :
        name_(name),
        ptest_function_(ptest_function)
    {
    }
    void operator()(no_context&)
    {
        (*ptest_function_)();
    }

    char const* name() const
    {
        return name_;
    }

private:
    char const* name_;
    void (*ptest_function_)();
};

class suite_base {
public:
    virtual ~suite_base() = 0;
    virtual void operator()() = 0;
    virtual std::size_t succeeded() const = 0;
    virtual std::size_t count() const = 0;
};

inline suite_base::~suite_base() {}

inline std::vector<suite_base*>& get_test_suites()
{
    static std::vector<suite_base*> test_suites;
    return test_suites;
}

inline void register_suite(suite_base* psuite)
{
    get_test_suites().push_back(psuite);
}

template <typename Context = no_context>
class suite : public suite_base {
public:
    suite(std::initializer_list<test<Context>> tests) :
        tests_(tests),
        succeeded_(0)
    {
        register_suite(this);
    }

    void operator()() override
    {
        Context c;
        for(test<Context>& t : tests_) {
            g_current_testcase = t.name();
            std::cout << t.name() << std::endl;
            try {
                t(c);
            } catch(std::exception const& e) {
                std::cout << "  " << e.what() << std::endl;
                continue;
            }
            ++succeeded_;
        }
    }

    std::size_t succeeded() const override
    {
        return succeeded_;
    }
    std::size_t count() const override
    {
        return tests_.size();
    }

private:
    std::vector<test<Context>> tests_;
    std::size_t succeeded_;
};

inline int run()
{
    std::size_t succeeded = 0;
    for(suite_base* psuite : get_test_suites()) {
        (*psuite)();
        if(psuite->succeeded() == psuite->count())
            ++succeeded;
    }
    return 0;
}

class error : public std::logic_error {
public:
    error(std::string const& expression, char const* file, unsigned line) :
        logic_error(make_what(expression, file, line)),
        expression_(expression),
        file_(file),
        line_(line)
    {
    }

private:
    static std::string make_what(std::string const& expression, char const* file, unsigned line)
    {
        std::ostringstream ostr;
        ostr << file << '(' << line << "): error in \"" << g_current_testcase << "\": test " << expression << " failed";
        return ostr.str();
    }

    std::string expression_;
    char const* file_;
    unsigned line_;
};

#define UNIT_TEST_MAIN() \
namespace unit_test { \
char const* g_current_testcase; \
} \
int main() \
{ \
    return unit_test::run(); \
} \
int main()



#define TEST(a) if(a) {} else throw unit_test::error(#a, __FILE__, __LINE__)
#define TESTCASE(name) {#name, &name}

}   // namespace unit_test
#endif

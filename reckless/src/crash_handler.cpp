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
#include <reckless/basic_log.hpp>
#include <reckless/crash_handler.hpp>

#include <cstring>  // memset
#include <vector>
#include <system_error>
#include <cassert>
#include <errno.h>
#include <signal.h> // sigaction

namespace reckless {
namespace {
// We set handlers for all signals that have a default behavior to
// terminate the process or generate a core dump (and that have not had
// their handler set to something else already).
std::initializer_list<int> const signals = {
    // POSIX.1-1990 signals
    SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGKILL, SIGSEGV,
    SIGPIPE, SIGALRM, SIGTERM, SIGUSR1, SIGUSR2,
    // SUSv2 + POSIX.1-2001 signals
    SIGBUS, SIGPOLL, SIGPROF, SIGSYS, SIGTRAP, SIGVTALRM, SIGXCPU, SIGXFSZ,
    // Various other signals
    SIGIOT, SIGSTKFLT, SIGIO, SIGPWR, SIGUNUSED,
    };

std::vector<basic_log*> g_logs;
std::vector<std::pair<int, struct sigaction>> g_old_sigactions;

void signal_handler(int)
{
    for(basic_log* plog : g_logs)
    {
        plog->panic_flush();
    }
}
}   // anonymous namespace

void install_crash_handler(std::initializer_list<basic_log*> log)
{
    assert(log.size() != 0);
    assert(g_logs.empty());
    g_logs.assign(begin(log), end(log));

    struct sigaction act;
    std::memset(&act, 0, sizeof(act));
    act.sa_handler = &signal_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_RESETHAND;
    // Some signals are synonyms for each other. Some are explictly specified
    // as such, but others may just be implemented that way on specific
    // systems. So we'll remove duplicate entries here before we loop through
    // all the signal numbers.
    std::vector<int> unique_signals(signals);
    sort(begin(unique_signals), end(unique_signals));
    unique_signals.erase(unique(begin(unique_signals), end(unique_signals)),
            end(unique_signals));
    try {
        g_old_sigactions.reserve(unique_signals.size());
        for(auto signal : unique_signals) {
            struct sigaction oldact;
            if(0 != sigaction(signal, nullptr, &oldact))
                throw std::system_error(errno, std::system_category());
            if(oldact.sa_handler == SIG_DFL) {
                if(0 != sigaction(signal, &act, nullptr))
                {
                    if(errno == EINVAL) {
                        // If we get EINVAL then we assume that the kernel
                        // does not know about this particular signal number.
                        continue;
                    }
                    throw std::system_error(errno, std::system_category());
                }
                g_old_sigactions.push_back({signal, oldact});
            }
        }
    } catch(...) {
        uninstall_crash_handler();
        throw;
    }
}

void uninstall_crash_handler()
{
    assert(!g_logs.empty());
    while(!g_old_sigactions.empty()) {
        auto const& p = g_old_sigactions.back();
        auto signal = p.first;
        auto const& oldact = p.second;
        if(0 != sigaction(signal, &oldact, nullptr))
            throw std::system_error(errno, std::system_category());
        g_old_sigactions.pop_back();
    }
    g_logs.clear();
}

}   // namespace reckless

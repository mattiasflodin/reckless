/* This file is part of reckless logging
 * Copyright 2015-2020 Mattias Flodin <git@codepentry.com>
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
#include <reckless/policy_log.hpp>
#include <reckless/writer.hpp>

#include "unreliable_writer.hpp"

#include <chrono>

void writer_error_callback(reckless::output_buffer* pbuffer, std::error_code ec,
        std::size_t lost_frames)
{
    char* p = pbuffer->reserve(128);
    int len = std::sprintf(p,
        "Failure %s:%d while writing to log; lost %d log records\n",
        ec.category().name(), ec.value(), static_cast<unsigned>(lost_frames));
    pbuffer->commit(len);
}

unreliable_writer writer;

void recovered_error()
{
    reckless::policy_log<> log(&writer);
    log.write("Successful write");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    std::cout << "Simulating disk full" << std::endl;
    // These should come through once the simulated disk is no longer full
    log.write("Temporary failed write #1");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    log.write("Temporary failed write #2");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Simulating disk no longer full" << std::endl;
    writer.error_code.clear();
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void recovered_error_with_registered_callback()
{
    reckless::policy_log<> log(&writer);
    log.write("Successful write");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    log.temporary_error_policy(reckless::error_policy::notify_on_recovery);
    log.writer_error_callback(&writer_error_callback);
    std::cout << "Simulating disk full" << std::endl;
    // These failed writes come through once the simulated disk is no longer
    // full writer_error_callback should not be called because no messages were
    // lost.
    log.write("Temporary failed write #1");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    log.write("Temporary failed write #2");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Simulating disk no longer full" << std::endl;
    writer.error_code.clear();
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void lost_frames_with_registered_callback()
{
    reckless::policy_log<> log(&writer, 0, 1024);
    log.temporary_error_policy(reckless::error_policy::notify_on_recovery);
    log.writer_error_callback(&writer_error_callback);
    std::cout << "Simulating disk full" << std::endl;
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    char const msg[] = "Temporary failed write";
    for(std::size_t count=0; count!=1024/sizeof(msg) + 1; ++count)
        log.write(msg);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Simulating disk no longer full" << std::endl;
    writer.error_code.clear();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    log.writer_error_callback();
}

void fail_immediately()
{
    reckless::policy_log<> log(&writer);
    log.temporary_error_policy(reckless::error_policy::fail_immediately);
    log.write("Successful write");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    try {
        log.write("Probably successful write");
        std::cout << "Write #1 generated no exception." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        log.write("Failed write");
        std::cout << "Write #2 generated no exception." << std::endl;
    } catch(reckless::writer_error const& e) {
        std::cout << "Exception " << e.what() << '(' << e.code() << ')' << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    writer.error_code.clear();
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void noexcept_on_close()
{
    // Check that close(error_code) does not throw an exception even if there is
    // a flush error.
    reckless::policy_log<> log(&writer);
    log.temporary_error_policy(reckless::error_policy::fail_immediately);
    log.write("Successful write");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    std::this_thread::sleep_for(std::chrono::seconds(2));
    try {
        log.write("Failed write");
    } catch(reckless::writer_error const& e) {
        std::cout << "Exception " << e.what() << '(' << e.code() << ')' << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    try {
        std::error_code error;
        std::cout << "call close()" << std::endl;
        log.close(error);
        std::cout << "close() -> " << error << std::endl;
    } catch(...) {
        std::cout << "error: close(error_code) throws exception" << std::endl;
    }
}

#define TEST(name) \
    std::cout << "-- " #name " ---------------" << std::endl; \
    name(); \
    std::cout << std::endl

int main()
{
    TEST(recovered_error);
    TEST(recovered_error_with_registered_callback);
    TEST(lost_frames_with_registered_callback);
    TEST(fail_immediately);
    TEST(noexcept_on_close);
    return 0;
}

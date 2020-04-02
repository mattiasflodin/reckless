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
#include <thread>   // sleep_for

#include "unreliable_writer.hpp"

unreliable_writer writer;
int main()
{
    reckless::policy_log<> log(&writer);
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    log.write("(2) Write by log");
    std::cout << "(1) Write by cout " << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    log.write("(3) Write by log, then flush");
    log.flush();
    std::cout << "(4) Write by cout after flush " << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    std::cout <<"Simulating disk full" << std::endl;
    log.write("(6) Write by log, then flush");
    try {
        log.flush();
    } catch(reckless::writer_error const& e) {
        std::cout << "(5) Catch writer_error " << e.code() << " on flush"
            << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout <<"Simulating disk no longer full" << std::endl;
    writer.error_code.clear();
    log.flush();
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout <<"Simulating disk full" << std::endl;
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    log.write("(8) Write by log, then flush");
    std::error_code error;
    log.flush(error);
    if(error) {
        std::cout << "(7) Obtain error code " << error << " on flush"
            << std::endl;
    }
    writer.error_code.clear();
    log.flush();

    return 0;
}

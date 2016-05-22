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
#include <reckless/severity_log.hpp>
#include <reckless/file_writer.hpp>

// It is possible to build custom loggers for various ways of formatting the
// log. The severity log is a stock policy-based logger that allows you to
// configure fields that should be put on each line, including a severity
// marker for debug/info/warning/error.
using log_t = reckless::severity_log<
    reckless::indent<4>,       // 4 spaces of indent
    ' ',                       // Field separator
    reckless::severity_field,  // Show severity marker (D/I/W/E) first
    reckless::timestamp_field  // Then timestamp field
    >;
    
reckless::file_writer writer("log.txt");
log_t g_log(&writer);

int main()
{
    std::string s("Hello World!");
    g_log.debug("Pointer: %p", s.c_str());
    g_log.info("Info line: %s", s);
    for(int i=0; i!=4; ++i) {
        reckless::scoped_indent indent;  // The indent object causes the lines
        g_log.warn("Warning: %d", i);  // within this scope to be indented
    }
    g_log.error("Error: %f", 3.14);

    return 0;
}

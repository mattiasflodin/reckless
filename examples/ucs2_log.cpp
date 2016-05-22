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


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
#include "mandelbrot.hpp"

#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <reckless/detail/trace_log.hpp>
#endif

#include <vector>
#include <thread>
#include <fstream>
#include <iostream>
#include <chrono>

#include LOG_INCLUDE

#ifdef ENABLE_PERFORMANCE_LOG
unsigned const SAMPLES_WIDTH = 512;
unsigned const SAMPLES_HEIGHT = 512;
#else
unsigned const SAMPLES_WIDTH = 1024;
unsigned const SAMPLES_HEIGHT = 1024;
#endif
unsigned const MAX_ITERATIONS = 32768;

double const BOX_LEFT = -0.69897762686014175;
double const BOX_TOP = 0.26043204963207245;
double const BOX_WIDTH = 1.33514404296875e-05;
double const BOX_HEIGHT = BOX_WIDTH*SAMPLES_HEIGHT/SAMPLES_WIDTH;

int main()
{
    std::vector<unsigned> sample_buffer(SAMPLES_WIDTH*SAMPLES_HEIGHT);
#ifndef ENABLE_PERFORMANCE_LOG
    auto start = std::chrono::steady_clock::now();
#endif
    {
        LOG_INIT();
        mandelbrot(&sample_buffer[0], SAMPLES_WIDTH, SAMPLES_HEIGHT,
            BOX_LEFT, BOX_TOP, BOX_LEFT+BOX_WIDTH, BOX_TOP-BOX_HEIGHT,
            MAX_ITERATIONS, THREADS);
        LOG_CLEANUP();
    }
#ifndef ENABLE_PERFORMANCE_LOG
    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count() << std::endl;
#endif

    char image[3*SAMPLES_WIDTH*SAMPLES_HEIGHT];
    color_mandelbrot(image, &sample_buffer[0], SAMPLES_WIDTH, SAMPLES_HEIGHT,
            MAX_ITERATIONS);

    std::ofstream ofs("bench_mandelbrot.data", std::ios::binary);
    ofs.write(image, sizeof(image));

#ifdef RECKLESS_ENABLE_TRACE_LOG
    std::ofstream trace_log("trace_log.txt", std::ios::trunc);
    reckless::detail::g_trace_log.read([&](std::string const& s) {
        trace_log << s << std::endl;
    });
#endif
    return 0;
}

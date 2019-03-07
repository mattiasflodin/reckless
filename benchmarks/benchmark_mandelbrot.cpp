#include "mandelbrot.hpp"

#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <performance_log/trace_log.hpp>
#endif

#include <vector>
#include <thread>
#include <fstream>
#include <iostream>
#include <chrono>

#include LOG_INCLUDE

unsigned const SAMPLES_WIDTH = 1024;
unsigned const SAMPLES_HEIGHT = 1024;
unsigned const MAX_ITERATIONS = 32768;

double const BOX_LEFT = -0.69897762686014175;
double const BOX_TOP = 0.26043204963207245;
double const BOX_WIDTH = 1.33514404296875e-05;
double const BOX_HEIGHT = BOX_WIDTH*SAMPLES_HEIGHT/SAMPLES_WIDTH;

int main()
{
    std::vector<unsigned> sample_buffer(SAMPLES_WIDTH*SAMPLES_HEIGHT);
    auto start = std::chrono::steady_clock::now();
    {
        LOG_INIT(8192);
        mandelbrot(&sample_buffer[0], SAMPLES_WIDTH, SAMPLES_HEIGHT,
            BOX_LEFT, BOX_TOP, BOX_LEFT+BOX_WIDTH, BOX_TOP-BOX_HEIGHT,
            MAX_ITERATIONS, THREADS);
        LOG_CLEANUP();
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count() << std::endl;

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

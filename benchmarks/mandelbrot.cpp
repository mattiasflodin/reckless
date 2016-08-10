#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <reckless/detail/trace_log.hpp>
#endif

#include <cstdlib>
#include <cstdint>
#include <thread>
#include <vector>
#include <mutex>
#include <ciso646>
#include <complex>
#include <algorithm>
#include <numeric>
#include <cmath>

#define LOG_ONLY_DECLARE
#include LOG_INCLUDE

#ifdef RECKLESS_ENABLE_TRACE_LOG
struct log_start_event
{
    log_start_event(unsigned char thread_index) :
        thread_index_(thread_index)
    {
    }

    std::string format() const
    {
        return std::string("log_start:") + std::to_string(static_cast<unsigned>(thread_index_));
    }
    unsigned char thread_index_;
};

struct log_finish_event
{
    log_finish_event(unsigned char thread_index) :
        thread_index_(thread_index)
    {
    }

    std::string format() const
    {
        return std::string("log_finish:") + std::to_string(static_cast<unsigned>(thread_index_));
    }

    unsigned char thread_index_;
};

#endif
namespace {

unsigned const THREAD_SLICE_SIZE = 1024*16;

void mandelbrot_thread(
    unsigned thread_index,
    unsigned* sample_buffer,
    unsigned samples_width,
    unsigned samples_height,
    double x1,
    double y1,
    double x2,
    double y2,
    unsigned max_iterations,
    std::mutex* next_slice_mutex,
    unsigned* next_slice_index)
{
    double const width = x2 - x1;
    double const height = y1 - y2;
    double const scale_x = width/samples_width;
    double const scale_y = height/samples_height;
    unsigned const total_samples = samples_width*samples_height;
    while(true) {
        std::size_t sample_index;
        std::size_t end_sample_index;
        {
            std::lock_guard<std::mutex> lk(*next_slice_mutex);
            sample_index = *next_slice_index;
            if(sample_index == total_samples)
                return;
            end_sample_index = std::min<std::size_t>(total_samples, sample_index + THREAD_SLICE_SIZE);
            *next_slice_index = static_cast<unsigned>(end_sample_index);
        }
        //printf("\r%.0f%%", 100.0*sample_index/total_samples);
        //fflush(stdout);

        while(sample_index != end_sample_index) {
            unsigned sample_x = sample_index % samples_width;
            unsigned sample_y = static_cast<unsigned>(sample_index / samples_width);
            std::complex<double> c(x1 + sample_x*scale_x, y1 - sample_y*scale_y);
            std::complex<double> z;

            unsigned iterations = 0;
            while(norm(z) < 2*2 and iterations != max_iterations) {
                z = z*z + c;
                ++iterations;
            }

#ifdef RECKLESS_ENABLE_TRACE_LOG
            RECKLESS_TRACE(log_start_event, static_cast<unsigned char>(thread_index));
#endif
            LOG_MANDELBROT(thread_index, sample_x, sample_y, c.real(), c.imag(), iterations);
#ifdef RECKLESS_ENABLE_TRACE_LOG
            RECKLESS_TRACE(log_finish_event, static_cast<unsigned char>(thread_index));
#endif

            sample_buffer[sample_index] = iterations;
            ++sample_index;
        }
    }
}

}   // anonymous namespace

void mandelbrot(
    unsigned* sample_buffer,
    unsigned samples_width,
    unsigned samples_height,
    double x1,
    double y1,
    double x2,
    double y2,
    unsigned max_iterations,
    unsigned thread_count)
{
    unsigned next_slice_index = 0;
    std::mutex next_slice_index_mutex;
    std::vector<std::thread> threads(thread_count);
    for(unsigned thread=0; thread!=thread_count; ++thread) {
        threads[thread] = std::thread(&mandelbrot_thread, thread, sample_buffer,
                samples_width, samples_height, x1, y1, x2, y2, max_iterations,
                &next_slice_index_mutex, &next_slice_index);
    }
    for(std::size_t thread=0; thread!=thread_count; ++thread)
        threads[thread].join();
}

void color_mandelbrot(char* image, unsigned const* sample_buffer,
        unsigned samples_width, unsigned samples_height,
        unsigned max_iterations)
{
    unsigned const PALETTE_SIZE = 256;
    std::uint8_t palette[3*PALETTE_SIZE];
    std::fill(palette, palette+sizeof(palette), 0);
    for(unsigned i=0; i!=PALETTE_SIZE; ++i) {
        double p=static_cast<double>(i)/PALETTE_SIZE;
        p *= p;
        double r, g, b;
        if(2*p < 1) {
            r = 2*p;
            g = p;
            b = 0;
        } else {
            r = 1;
            g = p;
            b = 2*p-1;
        }
        palette[3*i]   = static_cast<std::uint8_t>(255.0*r);
        palette[3*i+1] = static_cast<std::uint8_t>(255.0*g);
        palette[3*i+2] = static_cast<std::uint8_t>(255.0*b);
    }

    std::vector<std::size_t> histogram(max_iterations);
    for(std::size_t i=0; i!=samples_width*samples_height; ++i) {
        auto iterations = sample_buffer[i];
        histogram[iterations == max_iterations? 0 : iterations]++;
    }

    std::vector<std::uint8_t> hues(max_iterations);
    std::size_t hue = 0;
    for(std::size_t i=1; i!=max_iterations; ++i) {
        hue += histogram[i-1];
        hues[i] = static_cast<std::uint8_t>(
                static_cast<double>(PALETTE_SIZE-1)*hue/
                (samples_width*samples_height));
    }

    for(std::size_t i=0; i!=samples_width*samples_height; ++i)
    {
        auto iterations = sample_buffer[i];
        auto color_index = hues[iterations == max_iterations? 0 : iterations];
        image[3*i] = palette[3*color_index];
        image[3*i+1] = palette[3*color_index+1];
        image[3*i+2] = palette[3*color_index+2];
    }
}

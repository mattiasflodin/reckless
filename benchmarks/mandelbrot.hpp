#include <cstdlib>
#include <cstdint>
#include <thread>
#include <mutex>
#include <ciso646>
#include <complex>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <fstream>

void mandelbrot_thread(unsigned* sample_buffer,
        unsigned samples_width, unsigned samples_height,
        double x1, double y1, double x2, double y2,
        )
{
    unsigned const total_samples;
    while(true)
    {
        std::size_t sample_index;
        std::size_t end_sample_index;
        {
            std::lock_guard<std::mutex> lk(g_pixel_index_mutex);
            pixel_index = g_pixel_index;
            if(pixel_index == WIDTH_PIXELS*HEIGHT_PIXELS)
                return;
            end_pixel_index = std::min<std::size_t>(
                WIDTH_PIXELS*HEIGHT_PIXELS,
                pixel_index + THREAD_SLICE_SIZE);
            g_pixel_index = end_pixel_index;
        }
        printf("\r%.0f%%", 100.0*pixel_index/(WIDTH_PIXELS*HEIGHT_PIXELS));
        fflush(stdout);

        while(pixel_index != end_pixel_index) {
            unsigned pixel_y = pixel_index / WIDTH_PIXELS;
            unsigned pixel_x = pixel_index % WIDTH_PIXELS;
            std::complex<double> c(BOX_LEFT + pixel_x*BOX_WIDTH/WIDTH_PIXELS,
                    BOX_TOP + (HEIGHT_PIXELS-pixel_y)*BOX_HEIGHT/HEIGHT_PIXELS);
            std::complex<double> z;

            unsigned iterations = 0;
            while(norm(z) < 2*2 and iterations != MAX_ITERATIONS) {
                z = z*z + c;
                ++iterations;
            }
            if(iterations == MAX_ITERATIONS)
                iterations = 0;
            g_image[pixel_index] = iterations;

            ++pixel_index;
        }
    }
}

void mandelbrot()
{
    std::thread threads[THREAD_COUNT];
    for(std::size_t thread=0; thread!=THREAD_COUNT; ++thread)
        threads[thread] = std::thread(&mandelbrot_thread);
    for(std::size_t thread=0; thread!=THREAD_COUNT; ++thread)
        threads[thread].join();

    unsigned max_value = 0.0;
    std::size_t histogram[MAX_ITERATIONS];
    std::fill(histogram, histogram+MAX_ITERATIONS, 0);
    for(std::size_t i=0; i!=WIDTH_PIXELS*HEIGHT_PIXELS; ++i) {
        max_value = std::max(max_value, g_image[i]);
        histogram[g_image[i]]++;
    }
    
    unsigned const PALETTE_SIZE = 256;

    std::uint8_t hues[MAX_ITERATIONS];
    std::fill(hues, hues+MAX_ITERATIONS, 0);
    std::size_t hue = 0;
    for(std::size_t i=1; i!=MAX_ITERATIONS; ++i) {
        hue += histogram[i-1];
        hues[i] = static_cast<std::uint8_t>(
                static_cast<double>(PALETTE_SIZE-1)*hue/
                (WIDTH_PIXELS*HEIGHT_PIXELS));
    }

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
    
    std::uint8_t image[3*WIDTH_PIXELS*HEIGHT_PIXELS];
    for(std::size_t i=0; i!=WIDTH_PIXELS*HEIGHT_PIXELS; ++i)
    {
        auto hue = hues[g_image[i]];
        image[3*i] = palette[3*hue];
        image[3*i+1] = palette[3*hue+1];
        image[3*i+2] = palette[3*hue+2];
    }
    std::ofstream ofs("mandelbrot.data");
    ofs.write(reinterpret_cast<char*>(image), sizeof(image));
}

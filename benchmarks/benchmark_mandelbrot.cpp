#include "mandelbrot.hpp"

#include <thread>
#include <fstream>

unsigned const SAMPLES_WIDTH = 1024;
unsigned const SAMPLES_HEIGHT = 1024;
unsigned const MAX_ITERATIONS = 32768;

double const BOX_LEFT = -0.69897762686014175;
double const BOX_TOP = 0.26043204963207245;
double const BOX_WIDTH = 1.33514404296875e-05;
double const BOX_HEIGHT = BOX_WIDTH*SAMPLES_HEIGHT/SAMPLES_WIDTH;

int main()
{
    unsigned sample_buffer[SAMPLES_WIDTH*SAMPLES_HEIGHT];
    mandelbrot(sample_buffer, SAMPLES_WIDTH, SAMPLES_HEIGHT,
        BOX_LEFT, BOX_TOP, BOX_LEFT+BOX_WIDTH, BOX_TOP-BOX_HEIGHT,
        MAX_ITERATIONS, std::thread::hardware_concurrency());
    
    char image[3*SAMPLES_WIDTH*SAMPLES_HEIGHT];
    color_mandelbrot(image, sample_buffer, SAMPLES_WIDTH, SAMPLES_HEIGHT,
            MAX_ITERATIONS);
    
    std::ofstream ofs("bench_mandelbrot.data");
    ofs.write(image, sizeof(image));
    return 0;
}

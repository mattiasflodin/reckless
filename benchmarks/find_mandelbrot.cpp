#include "mandelbrot.hpp"
#include <thread>
#include <vector>
#include <fstream>
#include <cstdio>
#include <chrono>

unsigned const SAMPLES_WIDTH = 1024;
unsigned const SAMPLES_HEIGHT = 1024;
unsigned const SEARCH_BOX_WIDTH = 256;
unsigned const SEARCH_BOX_HEIGHT = 256;
unsigned const MAX_ITERATIONS = 2048;
//unsigned const MAX_INSIDE = 5*SEARCH_BOX_WIDTH*SEARCH_BOX_HEIGHT/100;

int main()
{
    double x = -2.5;
    double w = 3.5;
    double y = 0.5*w*SAMPLES_HEIGHT/SAMPLES_WIDTH;
    double h = 2*y;

    std::vector<unsigned> sample_buffer(SAMPLES_WIDTH*SAMPLES_HEIGHT);
    mandelbrot(sample_buffer.data(), SAMPLES_WIDTH, SAMPLES_HEIGHT,
            x, y, x+w, y-h, MAX_ITERATIONS,
            std::thread::hardware_concurrency());
    std::vector<std::uint8_t> total;
    unsigned zoom_count = 0;
    while(true) { //zoom_count != 10) {
        std::printf("zoom level %u\n", zoom_count);
        unsigned best_iterations = 0;
        unsigned best_sample_x = 0;
        unsigned best_sample_y = 0;
        for(unsigned i=0; i!=SAMPLES_WIDTH*SAMPLES_HEIGHT; ++i) {
            if(sample_buffer[i] == MAX_ITERATIONS)
                sample_buffer[i] = 0;
        }
                
        for(unsigned y1=0; y1!=SAMPLES_HEIGHT-SEARCH_BOX_HEIGHT; ++y1) {
            for(unsigned x1=0; x1!=SAMPLES_WIDTH-SEARCH_BOX_WIDTH; ++x1) {
                unsigned x2 = x1 + SEARCH_BOX_WIDTH;
                unsigned y2 = y1 + SEARCH_BOX_HEIGHT;
                unsigned total_iterations = 0;
                //unsigned total_inside = 0;
                for(unsigned y=y1; y!=y2; ++y) {
                    unsigned sample_offset = y*SAMPLES_WIDTH;
                    for(unsigned x=x1; x!=x2; ++x) {
                        unsigned iterations = sample_buffer[sample_offset + x]; 
                        //if(iterations == MAX_ITERATIONS)
                        //    total_inside += 1;
                        //else
                            total_iterations += iterations;
                    }
                }
                //if(total_inside < MAX_INSIDE && total_iterations > best_iterations)
                if(total_iterations > best_iterations)
                {
                    best_iterations = total_iterations;
                    best_sample_x = x1;
                    best_sample_y = y1;
                }
            }
        }
        
        x = x + best_sample_x*w/SAMPLES_WIDTH;
        y = y - best_sample_y*h/SAMPLES_HEIGHT;
        w = w*SEARCH_BOX_WIDTH/SAMPLES_WIDTH;
        h = h*SEARCH_BOX_HEIGHT/SAMPLES_HEIGHT;
        std::printf("x=%.17g\n"
            "y=%.17g\n"
            "w=%.17g\n"
            "h=%.17g\n"
            "sample_x=%u\n"
            "sample_y=%u\n"
            "\n",
            x, y, w, h,
            best_sample_x, best_sample_y);
        auto start = std::chrono::system_clock::now();
        mandelbrot(sample_buffer.data(), SAMPLES_WIDTH, SAMPLES_HEIGHT,
                x, y, x+w, y-h, MAX_ITERATIONS,
                std::thread::hardware_concurrency());
        auto end = std::chrono::system_clock::now();
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        printf("%lu ms\n", milliseconds);
        if(zoom_count > 4 && milliseconds > 5000)
            break;
        
        ++zoom_count;
    }
    std::vector<char> image(3*SAMPLES_WIDTH*SAMPLES_HEIGHT);
    color_mandelbrot(image.data(), sample_buffer.data(),
            SAMPLES_WIDTH, SAMPLES_HEIGHT, MAX_ITERATIONS);
    std::ofstream ofs("mandelbrot.data");
    ofs.write(image.data(), image.size());
    //std::ofstream ofs2("total.data");
    //ofs2.write(reinterpret_cast<char const*>(total.data()), total.size());
    return 0;
}

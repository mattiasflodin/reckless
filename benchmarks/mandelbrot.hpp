#include <cstdlib>
#include <cstdint>
#include <mutex>

std::size_t const WIDTH_PIXELS=640;
std::size_t const HEIGHT_PIXELS=480;
std::size_t const THREAD_COUNT = 2;

std::uint8_t g_image[WIDTH_PIXELS*HEIGHT_PIXELS];
std::size_t g_position = 0;
std::mutex g_position_mutex;

void mandelbrot_thread()
{
    std::size_t start_position;
    {
        std::lock_guard lk(g_position_mutex);
        start_position = g_position;
        g_position += 
}

void mandelbrot()
{
    std::thread threads[THREAD_COUNT];
    for(std::size_t thread=0; thread!=THREAD_COUNT; ++thread)
    {
        threads[thread] = std::thread(&mandelbrot_thread);
    }
}

#include "mandelbrot.hpp"

unsigned const WIDTH_PIXELS=1024;
unsigned const HEIGHT_PIXELS=768;
unsigned const MAX_ITERATIONS = 16384;
unsigned const THREAD_COUNT = 4;
unsigned const THREAD_SLICE_SIZE = 64;
double const IN_X1 = -0.74887649041642;
double const IN_X2 = -0.7488746477625337;
double const IN_Y1 = 0.0740314695595073;
double const IN_Y2 = 0.07403331221339358;

double const BOX_CENTER_X = (IN_X1+IN_X2)/2-0.0000001;
double const BOX_CENTER_Y = (IN_Y1+IN_Y2)/2+0.0000001;
double const BOX_WIDTH = (IN_X2 - IN_X1)/100;
double const BOX_HEIGHT = BOX_WIDTH*HEIGHT_PIXELS/WIDTH_PIXELS;
double const BOX_LEFT = BOX_CENTER_X - BOX_WIDTH/2;
double const BOX_TOP = BOX_CENTER_Y - BOX_HEIGHT/2;

unsigned g_image[WIDTH_PIXELS*HEIGHT_PIXELS];
std::size_t g_pixel_index = 0;
std::mutex g_pixel_index_mutex;

int main()
{
    mandelbrot();
    
}

void mandelbrot(
    unsigned* sample_buffer,
    unsigned samples_width,
    unsigned samples_height,
    double x1,
    double y1,
    double x2,
    double y2,
    unsigned max_iterations,
    unsigned thread_count
    );

void color_mandelbrot(char* image, unsigned const* sample_buffer,
    unsigned samples_width, unsigned samples_height, unsigned max_iterations);

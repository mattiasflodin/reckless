// The NanoLog documentation has a benchmark that includes reckless but shows
// very poor worst-case performance:
// https://github.com/Iyengar111/NanoLog/tree/40a53c36e0336af45f7664abeb939f220f78273e
// So we include that benchmark here for comparison. I haven't been able to
// reproduce the NanoLog author's numbers however.
//
// I removed the other libraries tested to make it easier to build, as I'm
// currently only interested in using this to reproduce the recklesss results.

#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <reckless/severity_log.hpp>
#include <reckless/file_writer.hpp>

/* Returns microseconds since epoch */
uint64_t timestamp_now()
{
    return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}

template <typename Function>
void run_log_benchmark(Function &&f, char const *const logger)
{
    int iterations = 100000;
    std::vector<uint64_t> latencies;
    char const *const benchmark = "benchmark";
    for (int i = 0; i < iterations; ++i)
    {
        uint64_t begin = timestamp_now();
        f(i, benchmark);
        uint64_t end = timestamp_now();
        latencies.push_back(end - begin);
    }
    std::sort(latencies.begin(), latencies.end());
    uint64_t sum = 0;
    for (auto l : latencies)
    {
        sum += l;
    }
    printf("%s percentile latency numbers in microseconds\n%9s|%9s|%9s|%9s|%9s|%9s|%9s|\n%9ld|%9ld|%9ld|%9ld|%9ld|%9ld|%9lf|\n", logger, "50th", "75th", "90th", "99th", "99.9th", "Worst", "Average", latencies[(size_t)iterations * 0.5], latencies[(size_t)iterations * 0.75], latencies[(size_t)iterations * 0.9], latencies[(size_t)iterations * 0.99], latencies[(size_t)iterations * 0.999], latencies[latencies.size() - 1], (sum * 1.0) / latencies.size());
}

template <typename Function>
void run_benchmark(Function &&f, int thread_count, char const *const logger)
{
    printf("\nThread count: %d\n", thread_count);
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(run_log_benchmark<Function>, std::ref(f), logger);
    }
    for (int i = 0; i < thread_count; ++i)
    {
        threads[i].join();
    }
}

int main()
{
    using log_t = reckless::severity_log<reckless::indent<4>, ' ', reckless::severity_field, reckless::timestamp_field>;

    reckless::file_writer writer("/tmp/reckless.txt");
    log_t g_log(&writer);

    auto reckless_benchmark = [&g_log](int i, char const *const cstr) { g_log.info("Logging %s%d%d%c%lf", cstr, i, 0, 'K', -42.42); };
    for (auto threads : {1, 2, 3, 4})
        run_benchmark(reckless_benchmark, threads, "reckless");

    return 0;
}

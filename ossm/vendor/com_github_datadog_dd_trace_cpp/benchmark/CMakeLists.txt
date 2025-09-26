# This defines an executable, `dd_trace_cpp-benchmark`, that's a Google
# Benchmark based program that is a microbenchmark for dd-trace-cpp.
#
# It's intended to be used as part of Datadog's internal benchmarking platform.
# See `../.gitlab/benchmarks.yml`.
add_executable(dd_trace_cpp-benchmark
    benchmark.cpp
    hasher.cpp
)

# Google Benchmark is included as a git submodule.
# It depends on Google Test, which it will download if this option is set.
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON)
# Don't build Google Benchmark's unit tests.
set(BENCHMARK_ENABLE_TESTING OFF)
add_subdirectory(google-benchmark)

target_link_libraries(dd_trace_cpp-benchmark benchmark::benchmark dd_trace_cpp-static)

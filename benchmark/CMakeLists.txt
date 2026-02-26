include(FetchContent)

# Disable Google Benchmark's own tests
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.4
)

FetchContent_MakeAvailable(benchmark)

# Include our benchmark utilities
include(BenchmarkUtils)

# ============================================================================
# Benchmark Targets
# ============================================================================

# MaskArea Transform Benchmark
# Tests: Element transform, container transform, pipeline execution
add_selective_benchmark(
    NAME MaskArea
    SOURCES 
        MaskArea.benchmark.cpp
    LINK_LIBRARIES 
        DataManager
    DEFAULT ON
)

# Configure for profiling tools
if(TARGET benchmark_MaskArea)
    configure_benchmark_for_profiling(
        TARGET benchmark_MaskArea
        ENABLE_PERF ON
        ENABLE_HEAPTRACK ON
    )
endif()

# Analog Load Benchmark
add_selective_benchmark(
    NAME AnalogLoad
    SOURCES
        AnalogLoad.benchmark.cpp
    LINK_LIBRARIES
        DataManager
    DEFAULT ON
)

if(TARGET benchmark_AnalogLoad)
    configure_benchmark_for_profiling(
        TARGET benchmark_AnalogLoad
        ENABLE_PERF ON
        ENABLE_HEAPTRACK ON
    )
endif()

# Print summary of configured benchmarks
print_benchmark_summary()

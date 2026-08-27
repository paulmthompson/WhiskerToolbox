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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ============================================================================
# Benchmark Targets
# ============================================================================

# ScatterPlot view interaction stress (heaptrack regression guard)
add_selective_benchmark(
    NAME ScatterPlotView
    STRESS_ONLY
    SOURCES
        ScatterPlotViewInteraction.benchmark.cpp
    LINK_LIBRARIES
        ScatterPlotWidget
        DataManager
        AnalogTimeSeries
        TensorData
        TimeFrame
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
    INCLUDE_DIRS
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/ScatterPlotWidget"
        "${CMAKE_SOURCE_DIR}/benchmark"
    DEFAULT ON
)

if(TARGET benchmark_ScatterPlotView)
    configure_benchmark_for_profiling(
        TARGET benchmark_ScatterPlotView
        ENABLE_HEAPTRACK ON
    )
endif()

# Optional Google Benchmark JSON runner (stress executables are excluded).
add_benchmark_suite_target()

# Local heaptrack baseline record / regression check (not CTest).
add_benchmark_regression_targets()

# Print summary of configured benchmarks
print_benchmark_summary()

# Ad-hoc debug/diagnostic executables (not benchmarks)
add_subdirectory(adhoc)

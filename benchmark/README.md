# WhiskerToolbox Benchmarking

This directory contains performance benchmarks using Google Benchmark.

## Quick Start

```bash
# Build benchmarks (enabled by default)
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release

# Run all benchmarks
./out/build/Clang/Release/benchmark/benchmark_MaskArea

# Run specific benchmark
./out/build/Clang/Release/benchmark/benchmark_MaskArea --benchmark_filter=Pipeline
```

## Available Benchmarks

### MaskArea Transform Benchmarks

Tests the performance of mask area calculation and pipeline execution:

- **Element-level**: Single mask area calculation
- **Container-level**: MaskData → RaggedAnalogTimeSeries transform
- **Pipeline**: Full transform chains with optimization comparisons
- **Baseline**: Iteration and computation overhead measurements

## Creating New Benchmarks

1. Create `MyFeature.benchmark.cpp` in this directory
2. Add to `CMakeLists.txt`:
   ```cmake
   add_selective_benchmark(
       NAME MyFeature
       SOURCES MyFeature.benchmark.cpp
       LINK_LIBRARIES DataManager
   )
   ```
3. Build and run!

## Documentation

See [`docs/developer/benchmarking.md`](../docs/developer/benchmarking.md) for:
- Complete benchmarking guide
- Profiling tool integration (perf, heaptrack, valgrind)
- Assembly inspection techniques
- Best practices and patterns

## CMake Options

Control which benchmarks are built:

```bash
# Disable all benchmarks
cmake -DENABLE_BENCHMARK=OFF ..

# Disable specific benchmark
cmake -DBENCHMARK_MASK_AREA=OFF ..
```

# Fuzzing Tests

This directory contains fuzzing tests for WhiskerToolbox using libFuzzer.

## Quick Start

### Building Fuzz Tests

```bash
# Configure with fuzzing enabled
cmake --preset linux-clang-debug -DENABLE_FUZZING=ON

# Build the project
cmake --build --preset linux-clang-debug
```

### Running Fuzz Tests

Run a specific fuzz target:
```bash
./out/build/Clang/Debug/bin/fuzz_analog_csv -max_total_time=60
```

Run with corpus:
```bash
./out/build/Clang/Debug/bin/fuzz_analog_csv \
    -max_total_time=300 \
    tests/fuzz/corpus/analog_csv/
```

Run all fuzz targets:
```bash
./tools/run_all_fuzzers.sh 300  # Run for 5 minutes
```

### Available Fuzz Targets

1. **fuzz_analog_csv**: Tests AnalogTimeSeries CSV parsing
   - Corpus: `corpus/analog_csv/`
   - Tests various delimiters, formats, and edge cases

2. **fuzz_point_csv**: Tests Point data CSV parsing
   - Corpus: `corpus/point_csv/`
   - Tests coordinate parsing, column configurations

3. **fuzz_point_json**: Tests Point data JSON configuration
   - Corpus: `corpus/point_json/`
   - Tests JSON parsing and configuration handling

## Directory Structure

```
tests/fuzz/
├── CMakeLists.txt          # CMake configuration for fuzz tests
├── README.md               # This file
├── fuzz_analog_csv.cpp     # Analog CSV fuzz target
├── fuzz_point_csv.cpp      # Point CSV fuzz target
├── fuzz_point_json.cpp     # Point JSON fuzz target
└── corpus/                 # Seed corpus files
    ├── analog_csv/         # Corpus for analog CSV fuzzing
    ├── point_csv/          # Corpus for point CSV fuzzing
    └── point_json/         # Corpus for point JSON fuzzing
```

## Corpus Files

Corpus files are seed inputs that guide the fuzzer. The fuzzer mutates these files to explore different code paths. Good corpus files should:

- Be minimal and focused
- Cover different valid input formats
- Include edge cases (empty, very small, special characters)

## Integration with CTest

Fuzz tests are integrated with CTest for smoke testing:

```bash
# Run fuzz tests through CTest (short duration)
cd out/build/Clang/Debug
ctest -R fuzz -V
```

Note: CTest runs fuzz tests for only 10 seconds each as a smoke test. For thorough fuzzing, run the targets directly.

## Adding New Fuzz Targets

To add a new fuzz target:

1. Create a new `.cpp` file in this directory with the fuzz target implementation
2. Add the target to `CMakeLists.txt` using `add_fuzz_target()`
3. Create a corpus directory in `corpus/` with seed files
4. Update this README with information about the new target

Example fuzz target structure:
```cpp
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Your fuzzing code here
    // Test the target function with the fuzzed data
    return 0;
}
```

## Documentation

For detailed information about fuzzing support, see:
- [Main Fuzzing Documentation](../../docs/fuzzing.md)
- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)

## Troubleshooting

**Fuzz target not found:**
- Ensure you configured with `-DENABLE_FUZZING=ON`
- Ensure you're using Clang compiler

**Slow fuzzing:**
- Reduce `-max_len` to limit input size
- Use `-jobs=N` for parallel fuzzing

**No crashes found:**
- That's good! It means your code is robust
- Consider running for longer periods
- Review corpus diversity

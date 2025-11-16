# Fuzzing Guide for WhiskerToolbox

## Overview

This guide explains how to use fuzzing to test the WhiskerToolbox C++ library. Fuzzing is an automated testing technique that feeds random or semi-random data to your code to discover bugs, crashes, and security vulnerabilities.

## What is Fuzzing?

Fuzzing (or fuzz testing) is a software testing technique that involves providing invalid, unexpected, or random data as inputs to a computer program. The program is then monitored for exceptions such as crashes, failing built-in code assertions, or potential memory leaks.

## Why Fuzzing for WhiskerToolbox?

WhiskerToolbox processes various data formats (CSV, JSON, HDF5, binary) and performs complex data transformations. Fuzzing helps ensure robustness by:

- **Finding edge cases**: Discovering inputs that cause crashes or undefined behavior
- **Validating parsers**: Testing CSV/JSON/binary parsers with malformed inputs
- **Security**: Identifying potential buffer overflows or memory corruption
- **Robustness**: Ensuring graceful handling of invalid data

## Fuzzing Infrastructure

### Fuzzer: libFuzzer

We use **libFuzzer**, which is integrated into Clang/LLVM. libFuzzer is an in-process, coverage-guided fuzzing engine that works by:

1. Generating mutated inputs based on code coverage feedback
2. Running the fuzz target function with each input
3. Tracking which code paths are exercised
4. Mutating inputs to explore new code paths

### Integration with Catch2

Our fuzzing infrastructure is integrated with the existing Catch2 test framework:

- Fuzz targets are separate executables that can be run independently
- Each fuzz target focuses on a specific parsing or processing function
- Corpus files (seed inputs) are provided to guide the fuzzer

## Building with Fuzzing Support

### Prerequisites

- Clang compiler (version 6.0 or later)
- CMake 3.25 or later

### Build Configuration

To build the fuzzing tests, enable fuzzing in CMake:

```bash
# Configure with fuzzing enabled
cmake --preset linux-clang-debug -DENABLE_FUZZING=ON

# Build the project
cmake --build --preset linux-clang-debug

# The fuzz test executables will be in the build directory
```

### Build Options

- `ENABLE_FUZZING=ON`: Enable building of fuzz targets
- `ENABLE_UI=OFF`: Recommended to disable UI for fuzzing builds
- `CMAKE_BUILD_TYPE=Debug`: Use Debug for better stack traces

## Running Fuzz Tests

### Basic Usage

Each fuzz target can be run directly:

```bash
# Run a specific fuzz target
./out/build/Clang/Debug/bin/fuzz_analog_csv

# Run with a time limit (e.g., 60 seconds)
./out/build/Clang/Debug/bin/fuzz_analog_csv -max_total_time=60

# Run with a specific corpus directory
./out/build/Clang/Debug/bin/fuzz_analog_csv tests/fuzz/corpus/analog_csv/
```

### libFuzzer Options

Common libFuzzer options:

- `-max_total_time=N`: Run for N seconds
- `-max_len=N`: Maximum length of test input (bytes)
- `-dict=file`: Use a dictionary file for mutations
- `-jobs=N`: Run N parallel fuzzing jobs
- `-workers=N`: Use N workers for parallel fuzzing
- `-only_ascii=1`: Only generate ASCII inputs
- `-timeout=N`: Timeout in seconds for a single run

### Example Workflow

```bash
# 1. Build with fuzzing enabled
cmake --preset linux-clang-debug -DENABLE_FUZZING=ON
cmake --build --preset linux-clang-debug

# 2. Run a fuzz target for 5 minutes
./out/build/Clang/Debug/bin/fuzz_analog_csv \
    -max_total_time=300 \
    tests/fuzz/corpus/analog_csv/

# 3. If a crash is found, minimize the crashing input
./out/build/Clang/Debug/bin/fuzz_analog_csv \
    -minimize_crash=1 \
    crash-<hash>

# 4. Test the minimized crash
./out/build/Clang/Debug/bin/fuzz_analog_csv crash-minimized
```

## Available Fuzz Targets

### CSV Parsers

1. **fuzz_analog_csv**: Tests AnalogTimeSeries CSV parser
   - Target: `load_analog_series_from_csv()` and `load(CSVAnalogLoaderOptions)`
   - Focus: Malformed CSV data, invalid numbers, delimiter issues

2. **fuzz_point_csv**: Tests Point CSV parser
   - Target: `load(CSVPointLoaderOptions)`
   - Focus: Invalid coordinates, column parsing issues

3. **fuzz_line_csv**: Tests Line CSV parser
   - Target: CSV line data loading
   - Focus: Multi-column CSV parsing, coordinate extraction

### JSON Parsers

4. **fuzz_point_json**: Tests Point JSON parser
   - Target: `load_into_PointData()` with JSON config
   - Focus: Malformed JSON, invalid field types

5. **fuzz_analog_json**: Tests AnalogTimeSeries JSON config
   - Target: JSON configuration parsing
   - Focus: Missing fields, type mismatches

## Corpus Management

### What is a Corpus?

A corpus is a collection of input files that guide the fuzzer. The fuzzer uses these as seeds and mutates them to explore the input space.

### Creating Good Corpus Files

Good corpus files should:

1. **Be minimal**: Small, focused examples
2. **Cover features**: Represent different valid input formats
3. **Be valid**: Start with valid inputs that parse correctly
4. **Be diverse**: Include various edge cases

### Example Corpus Files

**analog_csv corpus:**
```
tests/fuzz/corpus/analog_csv/
├── simple.csv          # Single column of numbers
├── two_column.csv      # Time and value columns
├── with_header.csv     # CSV with header row
├── negative.csv        # Negative values
└── scientific.csv      # Scientific notation
```

## Integration with CTest

Fuzzing tests are integrated with CTest but run separately:

```bash
# Run only fuzz tests with a short duration
ctest -R fuzz -V

# The fuzz tests in CTest run for a limited time (default: 10 seconds)
```

Note: CTest integration is for smoke testing. For thorough fuzzing, run the fuzz targets directly with longer timeouts.

## Continuous Fuzzing

### Local Continuous Fuzzing

For continuous fuzzing on your local machine:

```bash
# Run multiple fuzz targets in parallel
./tools/run_all_fuzzers.sh 3600  # Run for 1 hour
```

### CI Integration

The fuzzing tests can be integrated into CI for regression testing:

```yaml
# Example GitHub Actions workflow
- name: Run Fuzz Tests (short)
  run: |
    cmake --preset linux-clang-debug -DENABLE_FUZZING=ON
    cmake --build --preset linux-clang-debug
    cd out/build/Clang/Debug
    ctest -R fuzz -V
```

## Debugging Crashes

When a fuzz test finds a crash:

1. **Locate the crash file**: libFuzzer creates `crash-<hash>` files
2. **Minimize the input**: 
   ```bash
   ./fuzz_target -minimize_crash=1 crash-<hash>
   ```
3. **Reproduce**: Run with the crash file:
   ```bash
   ./fuzz_target crash-<hash>
   ```
4. **Debug**: Use debugger with the crash file:
   ```bash
   gdb --args ./fuzz_target crash-<hash>
   ```
5. **Fix the bug**: Update the code to handle the edge case
6. **Add to corpus**: Add the fixed case to the corpus to prevent regression

## Best Practices

1. **Start with valid inputs**: Corpus should contain valid examples
2. **Run regularly**: Integrate fuzzing into your development workflow
3. **Fix crashes promptly**: Don't ignore fuzzer findings
4. **Update corpus**: Add interesting inputs discovered by the fuzzer
5. **Use sanitizers**: Build with AddressSanitizer for better bug detection:
   ```bash
   cmake --preset linux-clang-debug \
         -DENABLE_FUZZING=ON \
         -DenableAddressSanitizer=ON
   ```

## Limitations

- **GUI components**: Fuzzing is not suitable for GUI testing
- **Long operations**: Fuzz targets should complete quickly (< 100ms)
- **External dependencies**: Avoid network, filesystem, or database dependencies
- **Determinism**: Fuzz targets must be deterministic

## Future Improvements

Potential enhancements to the fuzzing infrastructure:

- [ ] Structure-aware fuzzing for JSON (using protobuf-based definitions)
- [ ] Differential fuzzing (compare with alternative implementations)
- [ ] Fuzzing for image processing functions
- [ ] Integration with OSS-Fuzz for continuous fuzzing
- [ ] Fuzzing dictionaries for better coverage
- [ ] Coverage-guided fuzzing metrics in CI

## Resources

- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [Fuzzing C++ Code Tutorial](https://github.com/google/fuzzing/blob/master/tutorial/libFuzzerTutorial.md)
- [OSS-Fuzz](https://github.com/google/oss-fuzz) - Continuous fuzzing for open source
- [Sanitizers](https://github.com/google/sanitizers) - AddressSanitizer, UndefinedBehaviorSanitizer

## Troubleshooting

### "libFuzzer not found"

Ensure you're using Clang and have libFuzzer support:
```bash
clang++ --version  # Check version >= 6.0
```

### Slow fuzzing performance

- Reduce `-max_len` to limit input size
- Use `-jobs=N` for parallel fuzzing
- Ensure code is compiled with optimizations

### No new coverage

- Review and improve corpus diversity
- Add dictionary files for structured formats
- Check if the code path is reachable

## Contact

For questions or issues with fuzzing:
- Open an issue on GitHub
- Refer to the main project documentation

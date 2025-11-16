![build](https://github.com/paulmthompson/WhiskerToolbox/actions/workflows/linux_cmake.yml/badge.svg)
![build](https://github.com/paulmthompson/WhiskerToolbox/actions/workflows/mac_cmake.yml/badge.svg)
![build](https://github.com/paulmthompson/WhiskerToolbox/actions/workflows/windows_cmake.yml/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/paulmthompson/WhiskerToolbox/badge.svg?branch=main)](https://coveralls.io/github/paulmthompson/WhiskerToolbox?branch=main)

# Neuralyzer

## Documentation

https://paulmthompson.github.io/WhiskerToolbox/

## Testing

### Unit Tests

The project uses Catch2 for unit testing. To build and run tests:

```bash
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
ctest --preset linux-clang-release
```

### Fuzzing

The project includes comprehensive fuzzing support using libFuzzer to test the robustness of data parsing and processing functions. Fuzzing helps discover edge cases, crashes, and potential security issues.

**Quick Start:**

```bash
# Build with fuzzing enabled (requires Clang)
cmake --preset linux-clang-debug -DENABLE_FUZZING=ON
cmake --build --preset linux-clang-debug

# Run a specific fuzz target
./out/build/Clang/Debug/bin/fuzz_analog_csv -max_total_time=300

# Run all fuzz targets
./tools/run_all_fuzzers.sh 300
```

**Available Fuzz Targets:**
- `fuzz_analog_csv` - Tests AnalogTimeSeries CSV parsing
- `fuzz_point_csv` - Tests Point data CSV parsing
- `fuzz_point_json` - Tests Point data JSON configuration

For detailed information, see [docs/fuzzing.md](docs/fuzzing.md) and [tests/fuzz/README.md](tests/fuzz/README.md).

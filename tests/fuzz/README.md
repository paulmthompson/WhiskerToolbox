# Fuzz Testing with Google FuzzTest

This directory contains fuzz tests for the WhiskerToolbox project using [Google FuzzTest](https://github.com/google/fuzztest).

## Overview

Fuzz testing helps discover edge cases, crashes, and unexpected behaviors by automatically generating test inputs. The FuzzTest library integrates with Google Test and uses modern fuzzing techniques including coverage-guided fuzzing.

## Requirements

- **Clang compiler** - FuzzTest requires Clang for its instrumentation features
- **CMake option** - Build with `-DENABLE_FUZZ_TESTING=ON`
- **Sanitizers** - AddressSanitizer is automatically enabled with fuzz tests

## Building and Running

### Configure and Build

```bash
# Configure with Clang and enable fuzz testing
cmake --preset linux-clang-release -DENABLE_FUZZ_TESTING=ON -DENABLE_ORTOOLS=OFF

# Build
cmake --build --preset linux-clang-release
```

### Run Fuzz Tests

Fuzz tests are integrated into the CTest framework:

```bash
# Run all fuzz tests with CTest
ctest --preset linux-clang-release -R fuzz --output-on-failure

# Run a specific fuzz test directly
./out/build/Clang/Release/tests/fuzz/unit/DataManager/DigitalTimeSeries/fuzz_digital_event_series_json

# Run with specific fuzz configuration
./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline --fuzz=TransformPipelineFuzz.FuzzPipelineJsonStructure
```

### Fuzz Test Options

FuzzTest provides several command-line options:

```bash
# Run for a specific duration
./fuzz_test --fuzz_for=60s

# Filter out internal FuzzTest tests (like Utf8DecodeTest)
./fuzz_test --gtest_filter=-Utf8*

# Run specific test in fuzzing mode
./fuzz_test --fuzz=YourTestSuite.YourFuzzTest

# Use a specific seed corpus
./fuzz_test --fuzz_corpus_dir=/path/to/corpus

# Minimize a test case
./fuzz_test --fuzz_minimize_reproducer=reproducer_file
```

## Directory Structure

```
tests/fuzz/
├── CMakeLists.txt              # Main fuzz testing configuration
├── README.md                   # This file
├── corpus/                     # Seed inputs for fuzz tests
│   ├── unit/                   # Unit test corpus
│   └── integration/            # Integration test corpus
├── unit/                       # Unit-level fuzz tests
│   └── DataManager/
│       ├── DigitalTimeSeries/  # Digital event series JSON fuzzing
│       └── transforms/         # Transform-level fuzzing
└── integration/                # Integration-level fuzz tests
    └── fuzz_transform_pipeline.cpp  # Pipeline execution fuzzing
```

## Test Categories

### Unit Fuzz Tests

Located in `unit/`, these tests focus on individual components:

- **Digital Event Series JSON** (`unit/DataManager/DigitalTimeSeries/`)
  - Corrupted JSON structures
  - Invalid parameter values
  - Missing required fields
  - Edge cases in scaling and data type conversion

### Integration Fuzz Tests

Located in `integration/`, these tests focus on system-level behaviors:

- **Transform Pipeline** (`integration/fuzz_transform_pipeline.cpp`)
  - Random pipeline configurations
  - Variable substitution edge cases
  - Multi-phase execution
  - Invalid transform combinations

## Writing New Fuzz Tests

### Basic Template

```cpp
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

void FuzzMyFunction(const std::string& input) {
    try {
        // Call your function with fuzzed input
        MyFunction(input);
        // Should not crash
    } catch (const std::exception&) {
        // Exceptions are acceptable
    }
}
FUZZ_TEST(MyFuzzSuite, FuzzMyFunction);
```

### With Custom Domains

```cpp
void FuzzWithConstraints(int x, float y, const std::string& s) {
    // Test with constrained inputs
    MyFunction(x, y, s);
}
FUZZ_TEST(MyFuzzSuite, FuzzWithConstraints)
    .WithDomains(
        fuzztest::InRange(0, 100),
        fuzztest::InRange(-1.0f, 1.0f),
        fuzztest::StringOf(fuzztest::InRange('a', 'z')).WithMaxSize(50)
    );
```

### Add to CMake

```cmake
add_executable(fuzz_my_component
    fuzz_my_component.cpp
)

target_link_libraries(fuzz_my_component
    PRIVATE
        MyComponent
        gtest
        gmock
        fuzztest::fuzztest
)

target_link_options(fuzz_my_component
    PRIVATE
        -fsanitize=fuzzer
)

gtest_discover_tests(fuzz_my_component)
```

## Corpus Management

The `corpus/` directory contains seed inputs that guide the fuzzer:

- **Good seeds** help discover bugs faster
- **Diverse seeds** improve code coverage
- **Invalid seeds** test error handling

Add representative test cases for your component in the appropriate subdirectory.

## Continuous Integration

Fuzz tests can be integrated into CI pipelines:

```bash
# Quick smoke test (short duration)
ctest -R fuzz --timeout 60

# Extended fuzzing (nightly builds)
ctest -R fuzz --timeout 3600
```

## Troubleshooting

### Build Errors

- **"Fuzz testing requires Clang"** - Make sure you're using Clang compiler
- **Linker errors** - Ensure `-fsanitize=fuzzer` is in link options

### Runtime Issues

- **Slow execution** - FuzzTest is CPU-intensive; this is normal
- **Memory usage** - AddressSanitizer increases memory usage significantly
- **Crashes** - These are findings! Save the reproducer and fix the bug

### Reproducing Crashes

When a crash is found, FuzzTest saves a reproducer:

```bash
# Rerun the specific failing input
./fuzz_test --fuzz_reproduce_finding=crash-reproducer-file
```

## Further Reading

- [Google FuzzTest Documentation](https://github.com/google/fuzztest/blob/main/doc/overview.md)
- [FuzzTest Domains Reference](https://github.com/google/fuzztest/blob/main/doc/domains-reference.md)
- [Coverage-Guided Fuzzing](https://en.wikipedia.org/wiki/Fuzzing#Coverage-guided_fuzzing)

## Contributing

When adding new functionality:

1. Write fuzz tests for parsers, loaders, and complex logic
2. Add representative corpus files
3. Document any fuzzing-specific considerations
4. Ensure tests pass in CI before merging

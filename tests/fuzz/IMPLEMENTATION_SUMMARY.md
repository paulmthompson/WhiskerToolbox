# Fuzz Testing Implementation Summary

## Overview

This document summarizes the fuzz testing infrastructure added to WhiskerToolbox using Google's FuzzTest library.

## Changes Made

### 1. CMake Configuration

**File:** `CMakeLists.txt`
- Added `ENABLE_FUZZ_TESTING` CMake option (OFF by default)
- Added Clang compiler requirement check for fuzz testing
- Configured to only enable fuzz tests when using Clang

**File:** `tests/CMakeLists.txt`
- Added conditional inclusion of fuzz test subdirectory

### 2. Directory Structure

```
tests/fuzz/
├── CMakeLists.txt                     # Main fuzz test configuration
├── README.md                          # Comprehensive documentation
├── QUICKSTART.md                      # Quick start guide
├── .gitignore                         # Ignore generated artifacts
├── corpus/                            # Seed inputs for fuzzing
│   ├── README.md
│   ├── unit/
│   │   ├── digital_event_series_uint16.json
│   │   ├── digital_event_series_csv.json
│   │   └── digital_event_series_invalid.json
│   └── integration/
│       ├── simple_pipeline.json
│       ├── multi_phase_pipeline.json
│       └── invalid_pipeline.json
├── unit/                              # Unit-level fuzz tests
│   ├── CMakeLists.txt
│   └── DataManager/
│       ├── CMakeLists.txt
│       ├── DigitalTimeSeries/
│       │   ├── CMakeLists.txt
│       │   └── fuzz_digital_event_series_json.cpp
│       └── transforms/
│           └── CMakeLists.txt
└── integration/                       # Integration-level fuzz tests
    ├── CMakeLists.txt
    └── fuzz_transform_pipeline.cpp
```

### 3. Fuzz Tests Created

#### A. Digital Event Series JSON Fuzzing

**File:** `tests/fuzz/unit/DataManager/DigitalTimeSeries/fuzz_digital_event_series_json.cpp`

**Test Functions:**
1. `FuzzJsonStructure` - Tests with arbitrary JSON strings
2. `FuzzValidJsonStructure` - Tests with structured but fuzzy parameters
3. `FuzzCsvJsonStructure` - Tests CSV-specific configuration
4. `FuzzEventDataTypeString` - Tests string-to-enum conversion
5. `FuzzEventScaling` - Tests event scaling with various factors

**Coverage:**
- JSON parsing robustness
- Parameter validation
- Data type conversion edge cases
- Scale factor handling (including division by zero)
- Missing/invalid field handling

#### B. Transform Pipeline Fuzzing

**File:** `tests/fuzz/integration/fuzz_transform_pipeline.cpp`

**Test Functions:**
1. `FuzzPipelineJsonStructure` - Tests with arbitrary pipeline JSON
2. `FuzzPipelineSteps` - Tests with random step configurations
3. `FuzzMaskAreaTransform` - Focused test on specific transform
4. `FuzzVariableSubstitution` - Tests variable substitution system
5. `FuzzPipelineFileLoading` - Tests file loading with corrupted files
6. `FuzzPipelineMetadata` - Tests metadata handling

**Coverage:**
- Pipeline configuration parsing
- Multi-phase execution
- Variable substitution edge cases
- Invalid transform names and parameters
- Metadata handling

### 4. Corpus Files

Seed inputs created to guide the fuzzer:

**Unit Test Corpus:**
- Valid uint16 format configuration
- Valid CSV format configuration
- Invalid configuration (for error handling tests)

**Integration Test Corpus:**
- Simple single-step pipeline
- Multi-phase pipeline with variables
- Invalid pipeline configuration

### 5. Documentation

**Primary Documentation:**
- `tests/fuzz/README.md` - Comprehensive guide covering:
  - Building and running fuzz tests
  - Writing new fuzz tests
  - Corpus management
  - CI integration
  - Troubleshooting

**Quick Start:**
- `tests/fuzz/QUICKSTART.md` - Step-by-step guide for developers

**Corpus Documentation:**
- `tests/fuzz/corpus/README.md` - Guide for managing test corpus

**Developer Documentation:**
- `docs/developer/copilot.qmd` - Added fuzz testing section to developer tools reference

## Building and Running

### Build Command

```bash
cmake --preset linux-clang-release -DENABLE_FUZZ_TESTING=ON -DENABLE_ORTOOLS=OFF
cmake --build --preset linux-clang-release
```

### Run Commands

```bash
# Via CTest
ctest --preset linux-clang-release -R fuzz --output-on-failure

# Directly
./out/build/Clang/Release/tests/fuzz/unit/DataManager/DigitalTimeSeries/fuzz_digital_event_series_json
./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline
```

## Technical Details

### Dependencies
- **Google FuzzTest** - Fetched automatically via CMake FetchContent
- **Clang Compiler** - Required for fuzzing instrumentation
- **AddressSanitizer** - Automatically enabled to detect memory issues

### Compiler Flags
- `-fsanitize=address,fuzzer-no-link` for compilation
- `-fsanitize=address,fuzzer-no-link` for linking (non-main)
- `-fsanitize=fuzzer` for final executable linking

### Integration with Existing Tests
- Fuzz tests are conditionally compiled only when `ENABLE_FUZZ_TESTING=ON`
- Uses same test infrastructure as Catch2 tests (via CTest)
- Compatible with existing CMake presets

## Future Extensions

### Suggested Additional Fuzz Tests

1. **HDF5 Data Loading** - Fuzz HDF5 file parsing
2. **CSV Loaders** - More extensive CSV parsing tests
3. **Image/Video Loading** - Test FFmpeg wrapper with corrupted media
4. **Line/Point Data** - Fuzz geometric data structures
5. **Kalman Filter** - Test tracking algorithms with extreme inputs
6. **Mask Operations** - Fuzz image processing operations
7. **State Estimation** - Test particle filters and other estimators

### Corpus Expansion

- Add more real-world configuration files
- Include known edge cases from bug reports
- Generate adversarial inputs for specific components

### CI/CD Integration

```bash
# Short smoke test for every PR
ctest -R fuzz --timeout 60

# Extended fuzzing for nightly builds
ctest -R fuzz --timeout 3600
```

## Benefits

1. **Automated Bug Discovery** - Finds edge cases developers might miss
2. **Robustness** - Ensures code handles invalid inputs gracefully
3. **Regression Prevention** - Catches bugs introduced by changes
4. **Documentation** - Fuzz tests document expected error handling
5. **Continuous Testing** - Can run indefinitely to find rare bugs

## Notes

- Fuzz testing is CPU-intensive by design
- AddressSanitizer increases memory usage (2-3x)
- Requires Clang compiler (not GCC or MSVC)
- Currently only supports Linux builds (can be extended to macOS)
- Disabled by default to not slow down regular development builds

## References

- [Google FuzzTest Documentation](https://github.com/google/fuzztest)
- [Coverage-Guided Fuzzing](https://en.wikipedia.org/wiki/Fuzzing#Coverage-guided_fuzzing)
- [LLVM libFuzzer](https://llvm.org/docs/LibFuzzer.html)

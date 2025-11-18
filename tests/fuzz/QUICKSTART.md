# Quick Start Guide - Fuzz Testing

## Building with Fuzz Testing

1. **Configure with Clang and enable fuzz testing:**

```bash
cmake --preset linux-clang-release -DENABLE_FUZZ_TESTING=ON -DENABLE_ORTOOLS=OFF -DENABLE_TIME_TRACE=ON
```

Note: FuzzTest requires abseil, which is provided by vcpkg. If you encounter build issues with abseil, you can enable OR-Tools which also uses abseil: `-DENABLE_ORTOOLS=ON`.

2. **Build the project:**

```bash
cmake --build --preset linux-clang-release
```

This will fetch Google FuzzTest from GitHub and build all fuzz tests with AddressSanitizer.

## Running Fuzz Tests

### Quick Test (via CTest)

```bash
ctest --preset linux-clang-release -R fuzz --output-on-failure
```

### Run Specific Test

```bash
# Digital Event Series JSON fuzzing
./out/build/Clang/Release/tests/fuzz/unit/DataManager/DigitalTimeSeries/fuzz_digital_event_series_json

# Transform Pipeline fuzzing
./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline
```

### Extended Fuzzing Session

```bash
# Run for 5 minutes
./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline --fuzz_for=300s
```

## What Gets Tested

### Unit Tests
- **Digital Event Series JSON Loading** - Tests JSON parsing with:
  - Corrupted JSON structures
  - Invalid parameter values (negative channels, invalid scales)
  - Missing required fields
  - Edge cases in data type conversion

### Integration Tests
- **Transform Pipeline Execution** - Tests pipeline system with:
  - Random pipeline configurations
  - Invalid transform names and parameters
  - Variable substitution edge cases
  - Multi-phase execution scenarios

## Expected Output

Fuzz tests run continuously and report:
- Coverage information
- Inputs tested per second
- Any crashes or failures found

A typical successful run looks like:
```
[*] Iteration 1000: 1543 exec/s, 1234 new features
[*] Iteration 2000: 1621 exec/s, 45 new features
...
```

If a crash is found, FuzzTest will:
1. Save a reproducer file
2. Report the crash details
3. Exit with failure

## Troubleshooting

### "Fuzz testing requires Clang compiler"
Make sure you're using a Clang-based preset:
```bash
cmake --preset linux-clang-release ...
```

### Slow Execution
Fuzz testing is CPU-intensive and uses AddressSanitizer, which significantly slows execution. This is expected.

### Memory Usage
AddressSanitizer can use 2-3x more memory than normal execution. This is normal.

## Next Steps

- See `tests/fuzz/README.md` for comprehensive documentation
- Add more fuzz tests for your components
- Integrate into CI/CD pipelines
- Add seed files to `tests/fuzz/corpus/` to improve coverage

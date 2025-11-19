# Fuzz Testing Checklist

Use this checklist when building and running fuzz tests for the first time or after making changes.

## Initial Setup

- [ ] Verify you have Clang compiler installed
  ```bash
  clang++ --version
  ```

- [ ] Clean any previous builds (optional but recommended for first time)
  ```bash
  rm -rf out/build/Clang/Release
  ```

## Build

- [ ] Configure CMake with fuzz testing enabled
  ```bash
  cmake --preset linux-clang-release -DENABLE_FUZZ_TESTING=ON -DENABLE_ORTOOLS=OFF -DENABLE_TIME_TRACE=ON
  ```
  Note: If you get abseil-related errors, try with `-DENABLE_ORTOOLS=ON`

- [ ] Build the project
  ```bash
  cmake --build --preset linux-clang-release
  ```

- [ ] Verify fuzz test executables were built
  ```bash
  ls -la out/build/Clang/Release/tests/fuzz/unit/DataManager/DigitalTimeSeries/fuzz_digital_event_series_json
  ls -la out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline
  ```

## Test Execution

- [ ] Run all fuzz tests via CTest (quick smoke test)
  ```bash
  ctest --preset linux-clang-release -R fuzz --output-on-failure
  ```

- [ ] Run Digital Event Series JSON fuzz test directly
  ```bash
  ./out/build/Clang/Release/tests/fuzz/unit/DataManager/DigitalTimeSeries/fuzz_digital_event_series_json
  ```
  - Press Ctrl+C after a few seconds to stop
  - Should see "Iteration X: Y exec/s" messages

- [ ] Run Transform Pipeline fuzz test directly
  ```bash
  ./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline
  ```
  - Press Ctrl+C after a few seconds to stop
  - Should see fuzzing progress

## Extended Testing (Optional)

- [ ] Run with time limit (e.g., 60 seconds)
  ```bash
  ./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline --fuzz_for=60s
  ```

- [ ] Run specific fuzz test within a suite
  ```bash
  ./out/build/Clang/Release/tests/fuzz/integration/fuzz_transform_pipeline \
    --fuzz=TransformPipelineFuzz.FuzzPipelineJsonStructure
  ```

## Verification

- [ ] No crashes detected during test runs
- [ ] Fuzzer reports increasing iteration counts
- [ ] AddressSanitizer reports no memory errors
- [ ] All CTest fuzz tests pass

## If Issues Found

When a crash is discovered:

1. **Save the reproducer file**
   - FuzzTest automatically saves crash reproducers
   - Look for files like `crash-<hash>` in the working directory

2. **Reproduce the crash**
   ```bash
   ./fuzz_test --fuzz_reproduce_finding=crash-<filename>
   ```

3. **Debug with GDB**
   ```bash
   gdb --args ./fuzz_test --fuzz_reproduce_finding=crash-<filename>
   ```

4. **Fix the bug** in the source code

5. **Verify the fix**
   ```bash
   # Rebuild
   cmake --build --preset linux-clang-release
   
   # Test with the reproducer again
   ./fuzz_test --fuzz_reproduce_finding=crash-<filename>
   ```

6. **Run extended fuzzing** to ensure no similar issues
   ```bash
   ./fuzz_test --fuzz_for=300s
   ```

## Common Issues

### Build fails with "fuzz testing requires Clang"
- Solution: Make sure you're using `linux-clang-release` preset

### Tests run very slowly
- This is expected! AddressSanitizer and fuzzing are CPU-intensive
- Typical speed: 500-2000 executions per second

### High memory usage
- This is expected! AddressSanitizer uses 2-3x normal memory
- Monitor with `htop` or `top`

### Tests timeout in CI
- Reduce timeout or limit fuzz test duration
- Use `--fuzz_for=<duration>` to limit execution time

## Next Steps

After successful testing:

- [ ] Review the documentation in `README.md`
- [ ] Consider adding more fuzz tests for other components
- [ ] Add more seed files to `corpus/` directory
- [ ] Integrate into CI/CD pipeline
- [ ] Document any bugs found and fixed

## Resources

- Full documentation: `tests/fuzz/README.md`
- Quick start guide: `tests/fuzz/QUICKSTART.md`
- Template for new tests: `tests/fuzz/EXAMPLE_TEMPLATE.cpp`
- Implementation details: `tests/fuzz/IMPLEMENTATION_SUMMARY.md`

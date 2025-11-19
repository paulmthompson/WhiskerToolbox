# Fuzz Test Corpus

This directory contains seed inputs (corpus) for fuzz testing. The fuzzer will use these as starting points and mutate them to discover edge cases.

## Directory Structure

- `unit/` - Corpus files for unit-level fuzz tests
  - `digital_event_series_*.json` - Seed configurations for digital event series JSON parsing

- `integration/` - Corpus files for integration-level fuzz tests
  - `*_pipeline.json` - Seed pipeline configurations for transform pipeline testing

## Adding Corpus Files

When adding new fuzz tests, include representative seed inputs here:

1. **Valid inputs** - Typical valid configurations the system should handle
2. **Edge cases** - Boundary values, empty data, maximum sizes
3. **Invalid inputs** - Known failure cases that should be handled gracefully

The fuzzer will automatically mutate these seeds to discover new test cases.

## Corpus Evolution

As the fuzzer runs, it may discover interesting inputs that trigger new code paths. These can be saved and added to the corpus to improve test coverage over time.

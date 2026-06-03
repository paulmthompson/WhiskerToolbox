# Test Fixture Porting Guide: V1 to V2 Transform Testing

This guide describes how to share test data and expected results between V1 and V2 transform implementations. The preferred approach for **algorithm parity** is a **test vector** (one table row = input + parameters + expected output). Heavier **fixtures** (Catch2 `TEST_CASE_METHOD`, DataManager setup) remain useful when tests need pre-loaded DataManager state or multiple named objects.

## Overview

Goals when porting tests:

1. **Decouple test data** from transform-specific APIs (V1 free functions vs V2 `ElementRegistry`)
2. **Reuse the same cases** in both test binaries
3. **Encode expected outputs once** so V1/V2 drift is caught immediately
4. **Keep version-specific tests separate** — JSON formats, registry metadata, progress/cancellation, null-input edge cases

Choose an approach:

| Approach | Best for | Shared artifact |
|----------|----------|-----------------|
| **Test vector** | Container/element transforms with clear I/O (signal in → events out) | `fixtures/vectors/.../*_vectors.hpp` + optional `*_test_helpers.hpp` |
| **Scenario builders** | Input-only reuse (parameters still duplicated in tests) | `fixtures/scenarios/.../*_scenarios.hpp` |
| **DataManager fixture** | Many named objects, time frames, integration tests | `fixtures/*TestFixture.hpp` |

For new work, prefer **test vectors** over scenario-only headers when parameters and expected results are duplicated across V1 and V2.

## Directory Structure

```
tests/DataManager/fixtures/
├── builders/                          # AnalogTimeSeriesBuilder, MaskDataBuilder, ...
├── vectors/                           # Preferred: full I/O test rows
│   └── analog/
│       ├── analog_event_threshold_vectors.hpp
│       └── analog_event_threshold_test_helpers.hpp
├── scenarios/                         # Input-only factory functions (legacy / simple cases)
│   └── analog/
│       └── interval_threshold_scenarios.hpp
├── MaskAreaTestFixture.hpp            # DataManager-backed fixtures (when needed)
└── ...

src/DataManager/transforms/.../my_transform.test.cpp     # V1
src/TransformsV2/algorithms/MyTransform/MyTransform.test.cpp   # V2
```

Both `test_data_manager` and `test_TransformsV2` add `tests/DataManager` to their include path — no CMake change is required for new headers under `fixtures/vectors/`.

## Recommended: Test Vector Pattern

### Step 1: Extract rows from V1 tests

Each duplicated `SECTION` becomes one `Case` row: raw `values`/`times`, neutral parameters (`threshold`, `direction`, `lockout`), and `expected_event_times`.

```cpp
// tests/DataManager/fixtures/vectors/analog/my_transform_vectors.hpp
namespace my_transform_vectors {

enum class Direction { positive, negative, absolute };

struct Case {
    std::string_view name;              // Catch2 DYNAMIC_SECTION label
    std::string_view dm_key;            // DataManager key for JSON integration tests
    std::vector<float> values;
    std::vector<int> times;
  // ... version-neutral parameters ...
    std::vector<int> expected_event_times;  // empty => expect no output events
};

inline std::vector<Case> const& algorithmCases() {
    static std::vector<Case> const cases = { /* rows */ };
    return cases;
}

inline Case const* findCaseByDmKey(std::string_view dm_key) { /* ... */ }

} // namespace
```

Use `static std::vector<Case> const` inside a function (not `constexpr std::array`) when rows contain `std::vector` members.

### Step 2: Add shared helpers

Keep helpers free of V2 headers when only V1 tests need them; put V2 param adapters in the V2 test `.cpp` if the V1 target must not link TransformsV2.

```cpp
// tests/DataManager/fixtures/vectors/analog/my_transform_test_helpers.hpp
namespace my_transform_test {

inline std::shared_ptr<AnalogTimeSeries> buildAnalogTimeSeries(Case const& tc) {
    return AnalogTimeSeriesBuilder().withValues(tc.values).atTimes(tc.times).build();
}

inline ThresholdParams toThresholdParams(Case const& tc) { /* map neutral enum → V1 */ }

inline void requireEventTimes(DigitalEventSeries const& events,
                              std::span<int const> expected_times) {
    // RangeEquals on event.time()
}

} // namespace
```

### Step 3: Table-driven algorithm tests (V1 and V2)

```cpp
for (auto const& tc : my_transform_vectors::algorithmCases()) {
    if (!isHappyPathCase(tc.dm_key)) { continue; }  // optional filter
    DYNAMIC_SECTION(tc.name) {
        auto input = my_transform_test::buildAnalogTimeSeries(tc);
        auto const params = my_transform_test::toThresholdParams(tc);  // or V2 adapter

        auto result = event_threshold(input.get(), params);  // V1
        // auto result = registry.executeContainerTransform<...>(..., toV2Params(tc), ctx);  // V2

        my_transform_test::requireEventTimes(*result, tc.expected_event_times);
    }
}
```

Use **`DYNAMIC_SECTION(tc.name)`** so each row appears as its own subcase in Catch2 reports.

Split **happy path** vs **shared edge** loops with a small `isHappyPathCase(dm_key)` helper if edge rows should live in a separate `TEST_CASE`.

### Step 4: DataManager / JSON integration

Populate the DataManager from the same table:

```cpp
for (auto const& tc : my_transform_vectors::algorithmCases()) {
    std::string const time_key = std::string(tc.dm_key) + "_time";
    dm.setData(std::string(tc.dm_key), buildAnalogTimeSeries(tc), TimeKey(time_key));
}
```

JSON `input_key` values must match `Case::dm_key`. Assert pipeline output with `findCaseByDmKey("positive_no_lockout")` and `requireEventTimes`.

### Step 5: What stays out of the vector

| Keep in V1 test only | Keep in V2 test only |
|----------------------|----------------------|
| `ProgressCallback` sequencing | `ComputeContext::is_cancelled` |
| Null pointer / invalid enum handling | `loadParametersFromJson`, rfl validation |
| V1 `TransformRegistry` / `load_data_from_json_config` | `ElementRegistry` metadata |
| | `load_data_from_json_config_v2` |

Optional explicit **parity** `TEST_CASE` is usually unnecessary if both binaries iterate the same `algorithmCases()`.

## Alternative: Scenario Builders (input only)

When you only need shared **inputs**, use `fixtures/scenarios/` with builder functions:

```cpp
inline std::shared_ptr<AnalogTimeSeries> positive_threshold_no_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}
```

Downside: **parameters and expected outputs are still duplicated** in V1 and V2 test files. Migrate to a test vector when that duplication becomes painful (as with AnalogEventThreshold).

## Alternative: DataManager Fixture Class

Use `TEST_CASE_METHOD` fixtures when tests need a fully wired `DataManager`, multiple time keys, or many interdependent objects.

```cpp
class MyTransformTestFixture {
protected:
    MyTransformTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    DataManager* getDataManager() { return m_data_manager.get(); }
    std::map<std::string, std::shared_ptr<MyDataType>> m_test_data;

private:
    void populateTestData() {
        createData("single_element", /* ... */);
    }
    // ...
};
```

**Design principles:**

1. **Descriptive keys** — name by scenario, not by expected numeric result
2. **Document expectations in comments** on `populateTestData`, or prefer encoding them in a test vector
3. **No transform logic in the fixture** — data creation only

## Fixture Patterns (legacy / specialized)

### Multiple data types

```cpp
class MultiInputTestFixture {
protected:
    std::map<std::string, std::shared_ptr<TypeA>> m_type_a_data;
    std::map<std::string, std::shared_ptr<TypeB>> m_type_b_data;
};
```

### Parameterized data without a vector

Base signal in the fixture; parameter variations stay in each test file (acceptable only for a few cases).

## Checklist for Porting

- [ ] List duplicated V1 `SECTION`s (input, params, expected output)
- [ ] Add `fixtures/vectors/.../*_vectors.hpp` with `Case` + `algorithmCases()`
- [ ] Add `*_test_helpers.hpp` (`build*`, `to*Params`, shared matchers)
- [ ] Refactor V1 algorithm tests: loop + `DYNAMIC_SECTION`
- [ ] Refactor V2 algorithm tests: same loop, V2 param adapter + registry/direct call
- [ ] Wire DataManager/JSON tests from `dm_key` / `findCaseByDmKey`
- [ ] Leave version-specific API tests in the respective `.cpp` files
- [ ] Run V1 and V2 tests; remove obsolete scenario headers if fully migrated

## Example: AnalogEventThreshold

Reference implementation (test vector + helpers + table-driven tests):

| File | Role |
|------|------|
| [`analog_event_threshold_vectors.hpp`](../../tests/DataManager/fixtures/vectors/analog/analog_event_threshold_vectors.hpp) | `Case` rows: `values`, `times`, `threshold`, `direction`, `lockout`, `expected_event_times`, `dm_key` |
| [`analog_event_threshold_test_helpers.hpp`](../../tests/DataManager/fixtures/vectors/analog/analog_event_threshold_test_helpers.hpp) | `buildAnalogTimeSeries`, `toThresholdParams`, `requireEventTimes` (V2 `toAnalogEventThresholdParams` lives in the V2 test `.cpp`) |
| [`analog_event_threshold.test.cpp`](../../src/DataManager/transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.test.cpp) | V1: `algorithmCases()` loop; separate cases for progress, null input, invalid enum |
| [`AnalogEventThreshold.test.cpp`](../../src/TransformsV2/algorithms/AnalogEventThreshold/AnalogEventThreshold.test.cpp) | V2: same loop via registry; cancellation, JSON, registry tests separate |

**Sample row** (positive threshold, no lockout):

```cpp
{.name = "positive_no_lockout",
 .dm_key = "positive_no_lockout",
 .values = {0.5f, 1.5f, 0.8f, 2.5f, 1.2f},
 .times = {100, 200, 300, 400, 500},
 .threshold = 1.0f,
 .direction = Direction::positive,
 .lockout = 0.0f,
 .expected_event_times = {200, 400, 500}},
```

**Parity note:** V1 and V2 absolute mode compare `abs(value)` to `threshold` vs `abs(threshold)` respectively. Shared vectors use positive absolute thresholds only; add a dedicated row before testing negative absolute thresholds.

## Related follow-up

The same vector pattern can replace input-only [`interval_threshold_scenarios.hpp`](../../tests/DataManager/fixtures/scenarios/analog/interval_threshold_scenarios.hpp) for AnalogIntervalThreshold when parameter/expected duplication becomes a maintenance burden.

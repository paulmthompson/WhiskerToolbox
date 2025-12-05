# Test Fixture Porting Guide: V1 to V2 Transform Testing

This guide describes the process for creating reusable test fixtures that enable consistent testing between V1 and V2 transform implementations. By decoupling test data creation from the transform-specific API, you can verify that both V1 and V2 produce identical results for the same input data.

## Overview

The goal is to:
1. **Decouple test data** from transformation logic
2. **Reuse the same test data** across V1 and V2 tests
3. **Ensure algorithmic consistency** between implementations
4. **Keep parameter/JSON testing** in their respective test files

## Directory Structure

```
tests/
└── DataManager/
    └── fixtures/
        ├── MaskAreaTestFixture.hpp      # Shared test data
        ├── AnalogEventThresholdTestFixture.hpp
        └── ...

src/DataManager/
├── transforms/
│   └── Masks/
│       └── Mask_Area/
│           └── mask_area.test.cpp       # V1 tests (uses fixture)
└── transforms/v2/
    └── algorithms/
        └── MaskArea/
            └── MaskArea.test.cpp        # V2 tests (uses same fixture)
```

## Step-by-Step Process

### Step 1: Analyze Existing V1 Tests

Identify the test data patterns in your V1 tests:

```cpp
// Before: Data creation scattered in V1 tests
TEST_CASE("Mask area calculation", "[mask][area]") {
    auto mask_data = std::make_shared<MaskData>();
    
    // Data creation mixed with test logic
    std::vector<uint32_t> x1 = {1, 2, 3};
    std::vector<uint32_t> y1 = {1, 2, 3};
    mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x1, y1), NotifyObservers::No);
    
    auto result = area(mask_data.get());
    // ... assertions
}
```

### Step 2: Create the Test Fixture

Create a fixture header in `tests/DataManager/fixtures/`:

```cpp
// tests/DataManager/fixtures/MyTransformTestFixture.hpp
#ifndef MY_TRANSFORM_TEST_FIXTURE_HPP
#define MY_TRANSFORM_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"
#include "DataManager.hpp"
#include "MyDataType.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <map>
#include <string>

class MyTransformTestFixture {
protected:
    MyTransformTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~MyTransformTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    // Map of named test data objects
    std::map<std::string, std::shared_ptr<MyDataType>> m_test_data;

private:
    void populateTestData() {
        // Create each test scenario with a descriptive key
        createData("empty_data", /* ... */);
        createData("single_element", /* ... */);
        createData("multiple_elements", /* ... */);
        createData("edge_case_scenario", /* ... */);
    }

    void createData(const std::string& key, /* ... */) {
        auto data = std::make_shared<MyDataType>();
        // ... populate data ...
        m_data_manager->setData(key, data, TimeKey("default"));
        m_test_data[key] = data;
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

#endif
```

**Key Design Principles:**

1. **Use descriptive keys** - Name test data by the scenario, not the expected result
2. **Store both in map and DataManager** - Allows direct access and key-based lookup
3. **Document expected results in comments** - But don't encode them in the fixture
4. **Keep fixture minimal** - Only data creation, no transform logic

### Step 3: Refactor V1 Tests to Use Fixture

Update the V1 test file to use `TEST_CASE_METHOD`:

```cpp
// src/DataManager/transforms/.../my_transform.test.cpp

#include "my_transform.hpp"
#include "fixtures/MyTransformTestFixture.hpp"

// Tests using fixture data
TEST_CASE_METHOD(MyTransformTestFixture, 
                 "Transform - Scenario Name", 
                 "[transforms][v1][my_transform]") {
    
    auto input = m_test_data["single_element"];
    auto result = my_transform(input.get());
    
    REQUIRE(result != nullptr);
    // ... assertions specific to this scenario
}

// Keep non-fixture tests for simple cases
TEST_CASE("Transform - Simple inline test", "[transforms][v1][my_transform]") {
    // Small inline tests that don't benefit from fixture
}

// Keep parameter/JSON tests separate
TEST_CASE("Transform - JSON pipeline", "[transforms][v1][json]") {
    // JSON/parameter-specific tests stay in this file
}
```

### Step 4: Create V2 Tests Using Same Fixture

In the V2 test file, use the same fixture:

```cpp
// src/DataManager/transforms/v2/algorithms/MyTransform/MyTransform.test.cpp

#include "MyTransform.hpp"
#include "fixtures/MyTransformTestFixture.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// V2 tests using same fixture data
TEST_CASE_METHOD(MyTransformTestFixture,
                 "TransformsV2 - Scenario Name",
                 "[transforms][v2][my_transform]") {
    
    auto input = m_test_data["single_element"];
    
    // Use V2 registry
    auto& registry = ElementRegistry::instance();
    MyTransformParams params;
    
    auto result = registry.execute<ElementType, OutputType, MyTransformParams>(
        "MyTransform", *input, params);
    
    // Same assertions as V1 (verify algorithmic parity)
    REQUIRE(result == expected_value);
}

// V2 JSON/DataManager integration tests
TEST_CASE_METHOD(MyTransformTestFixture,
                 "TransformsV2 - DataManager Integration via load_data_from_json_config_v2",
                 "[transforms][v2][datamanager]") {
    
    // Fixture pre-populates data in DataManager
    auto dm = getDataManager();
    
    // V2 JSON format
    std::string json_config = R"({
        "name": "Test Pipeline",
        "steps": [{
            "name": "MyTransform",
            "input_key": "single_element",
            "output_key": "result",
            "parameters": {}
        }]
    })";
    
    auto result = load_data_from_json_config_v2(dm, json_config);
    REQUIRE(result.success);
    
    auto output = dm->getData<OutputType>("result");
    REQUIRE(output != nullptr);
    // ... verify results match V1
}
```

### Step 5: Verify Consistency

Create explicit parity tests:

```cpp
TEST_CASE_METHOD(MyTransformTestFixture,
                 "V1/V2 Parity - Single element",
                 "[transforms][parity]") {
    
    auto input = m_test_data["single_element"];
    
    // V1 result
    auto v1_result = v1_transform(input.get());
    
    // V2 result
    auto& registry = ElementRegistry::instance();
    auto v2_result = registry.execute<...>("Transform", *input, {});
    
    // Compare (accounting for V1->AnalogTimeSeries vs V2->RaggedAnalogTimeSeries)
    REQUIRE(/* results equivalent */);
}
```

## Fixture Patterns

### Pattern: Multiple Data Types

For transforms that work on different input types:

```cpp
class MultiInputTestFixture {
protected:
    std::map<std::string, std::shared_ptr<TypeA>> m_type_a_data;
    std::map<std::string, std::shared_ptr<TypeB>> m_type_b_data;
};
```

### Pattern: Expected Results Documentation

Document expected results without encoding them:

```cpp
void populateTestData() {
    // Scenario: Single mask at timestamp 100 with 3 pixels
    // V1 Expected: AnalogTimeSeries with {100: 3.0}
    // V2 Expected: RaggedAnalogTimeSeries with {100: [3.0]}
    createMask("single_mask_3px", 
        /*timestamp=*/100, 
        /*pixels=*/{Point2D{1,1}, Point2D{1,2}, Point2D{2,1}});
}
```

### Pattern: Parameterized Data

For testing parameter variations:

```cpp
void populateTestData() {
    // Base data for parameter testing
    createSignal("param_test_base", {1.0, 2.0, 3.0}, {0, 10, 20});
    
    // The parameters themselves stay in the test files,
    // only the data is shared via fixture
}
```

## What Stays in Test Files

Keep these in the respective V1/V2 test files:

1. **Parameter struct tests** - JSON loading, validation
2. **Transform-specific API tests** - V1 `TransformOperation` vs V2 `ElementRegistry`
3. **JSON pipeline format tests** - V1 format vs V2 format
4. **Registry-specific tests** - Metadata, type mapping
5. **Simple inline tests** - Trivial cases that don't need fixture

## Checklist for Porting

- [ ] Identify reusable test scenarios in V1 tests
- [ ] Create fixture in `tests/DataManager/fixtures/`
- [ ] Add fixture to V1 test's CMakeLists.txt includes
- [ ] Refactor V1 tests to use `TEST_CASE_METHOD`
- [ ] Verify V1 tests still pass
- [ ] Add fixture include to V2 test file
- [ ] Create V2 tests using same data keys
- [ ] Create DataManager integration tests using `load_data_from_json_config_v2`
- [ ] Verify V2 tests pass with same expected results
- [ ] (Optional) Add explicit V1/V2 parity tests

## Example: AnalogEventThreshold

See the complete implementation:
- Fixture: `tests/DataManager/fixtures/AnalogEventThresholdTestFixture.hpp`
- V2 Tests: `src/DataManager/transforms/v2/algorithms/AnalogEventThreshold/AnalogEventThreshold.test.cpp`

The fixture creates named test signals like `"positive_no_lockout"`, `"negative_with_lockout"`, etc., which are then used in both V1 and V2 tests with their respective APIs.

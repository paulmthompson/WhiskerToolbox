# Pipeline Value Store Refactoring Roadmap

## Overview

This document outlines the refactoring plan to replace the current `PreprocessingRegistry` and `ContextInjectorRegistry` systems with a unified **Pipeline Value Store** pattern. This pattern leverages reflect-cpp's JSON serialization to enable generic parameter binding from computed values.

### Goals

1. **Eliminate header dependency explosion** - No central header that must include all preprocessable param types
2. **Remove specialized context structs** - Replace `TrialContext`, `ComputedStatistics` with generic key-value store
3. **Enable composable pipelines** - Allow JSON pipelines to wire reduction outputs into transform params
4. **Leverage existing infrastructure** - Use reflect-cpp serialization already in place for all params

### Current Pain Points

| Issue | Current Approach | Problem |
|-------|-----------------|---------|
| Preprocessing | `PreprocessingRegistry` + `RegisteredTransforms.hpp` | Central header must include all preprocessable types |
| Context injection | `ContextInjectorRegistry` + `TrialContext` struct | Specialized struct, requires registration per type |
| Adding new transform | Modify `RegisteredTransforms.hpp`, add to `tryAllRegisteredPreprocessing()` | Breaks modularity, causes full rebuild |

### Folder Structure

Throughout the roadmap, files are organized into these categories:

| Folder | Purpose | Examples |
|--------|---------|----------|
| **`core`** | Public API for applications/users | `PipelineValueStore`, `TransformPipeline`, `PipelineLoader` |
| **`extension`** | Transform developer utilities | `ParameterBinding.hpp`, transform parameter types, binding declarations |
| **`detail`** | Pipeline execution internals | `PipelineStep`, `ReductionStep`, execution mechanisms |
| **`algorithms`** | Transform algorithms | `ZScoreNormalization`, `StandardReductions`, algorithm-specific params |

### Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    PipelineValueStore                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  "mean" → 0.5f, "std" → 0.1f, "alignment_time" → 100   │   │
│  └─────────────────────────────────────────────────────────┘   │
│        ↑                              ↓                         │
│   Range Reductions               Param Bindings                 │
│   (compute scalars)          (inject into params)               │
└─────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Core Infrastructure

**Goal**: Create the generic value store and binding mechanism without breaking existing code.

**Status**: ✅ COMPLETED

The core V2 infrastructure has been successfully implemented with all deliverables completed and tests passing:

**Key Features Implemented:**
- Generic `PipelineValueStore` for storing and accessing scalar values (float, int64_t, string)
- JSON serialization support for parameter binding via reflect-cpp
- Type-safe bindings with automatic type conversions
- Registry-based binding applicators for type-erased runtime dispatch
- `PipelineStep` extended with `param_bindings` field and `applyBindings()` method
- `ReductionStep` struct for representing reduction operations
- Comprehensive unit tests with 40+ test cases covering:
  - Type-safe storage and retrieval
  - JSON serialization
  - Type conversions and error handling
  - Query and mutation operations
  - Registry-based binding application

**Implementation Details:**

The implementation follows these design principles:

### 1.1 Create PipelineValueStore Class

**File**: `src/DataManager/transforms/v2/core/PipelineValueStore.hpp`

*Location: **core** - Public API used by both applications and transform developers*

```cpp
/**
 * @brief Runtime key-value store for pipeline intermediate values
 * 
 * Stores named scalar values (float, int, int64_t, string) that can be
 * bound to transform parameters. Uses JSON string representation as
 * interchange format for maximum flexibility with reflect-cpp.
 */
class PipelineValueStore {
public:
    // Type-safe setters
    void set(std::string const& key, float value);
    void set(std::string const& key, int value);
    void set(std::string const& key, int64_t value);
    void set(std::string const& key, std::string value);
    
    // Get as JSON fragment (for binding)
    std::optional<std::string> getJson(std::string const& key) const;
    
    // Typed getters (for direct access)
    std::optional<float> getFloat(std::string const& key) const;
    std::optional<int64_t> getInt(std::string const& key) const;
    
    // Check existence
    bool contains(std::string const& key) const;
    
    // Merge another store (for nested pipelines)
    void merge(PipelineValueStore const& other);
    
    // Clear all values
    void clear();
    
private:
    std::unordered_map<std::string, std::string> json_values_;
    std::unordered_map<std::string, std::variant<float, int64_t, std::string>> typed_values_;
};
```

### 1.2 Create Parameter Binding Utilities

**File**: `src/DataManager/transforms/v2/extension/ParameterBinding.hpp`

*Location: **extension** - Transform developer utilities for parameter binding*

```cpp
/**
 * @brief Apply value store bindings to parameters using reflect-cpp
 * 
 * Bindings map param field names to value store keys.
 * Uses JSON as the interchange format.
 */
template<typename Params>
Params applyBindings(
    Params const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store);

/**
 * @brief Type-erased binding application (for pipeline runtime)
 */
std::any applyBindingsErased(
    std::type_index params_type,
    std::any const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store);
```

### 1.3 Update PipelineStep with Bindings

**File**: `src/DataManager/transforms/v2/detail/PipelineStep.hpp`

*Location: **detail** - Pipeline execution implementation internals*

Add binding support to `PipelineStep`:

```cpp
struct PipelineStep {
    std::string transform_name;
    mutable std::any params;
    
    // NEW: Bindings from value store to param fields
    // Key: param field name, Value: store key
    std::map<std::string, std::string> param_bindings;
    
    // ... existing methods ...
    
    // NEW: Apply bindings before execution
    void applyBindings(PipelineValueStore const& store) const;
};
```

### 1.4 Add ReductionStep Struct

**File**: `src/DataManager/transforms/v2/detail/ReductionStep.hpp`

*Location: **detail** - Pipeline execution implementation internals*

```cpp
/**
 * @brief Represents a reduction that computes a value for the store
 */
struct ReductionStep {
    std::string reduction_name;    // Registry name (e.g., "Mean", "StdDev")
    std::string output_key;        // Store key for result
    std::any params;               // Reduction parameters
};
```

### 1.5 Deliverables

- [x] `PipelineValueStore.hpp` with implementation
- [x] `ParameterBinding.hpp` with reflect-cpp integration
- [x] Updated `PipelineStep.hpp` with `param_bindings` field
- [x] `ReductionStep.hpp` for reduction value storage
- [x] Unit tests for value store and bindings

---

## Phase 2: TransformPipeline Integration

**Goal**: Enable pipelines to compute reductions and bind results to params.

### 2.1 Add Reductions to TransformPipeline

Update `TransformPipeline` to support computing values before transform execution:

```cpp
class TransformPipeline {
public:
    /**
     * @brief Add a reduction step that computes a value for the store
     * 
     * @param reduction_name Name of registered range reduction
     * @param output_key Key to store result under
     * @param params Optional reduction parameters
     */
    TransformPipeline& addReduction(
        std::string const& reduction_name,
        std::string const& output_key,
        std::any params = {});
    
    /**
     * @brief Add multiple reductions at once
     */
    TransformPipeline& addReductions(
        std::vector<ReductionStep> reductions);
    
private:
    std::vector<ReductionStep> reductions_;  // NEW
    std::vector<PipelineStep> steps_;
};
```

### 2.2 Update Execute Methods

Modify `execute()` to populate store and apply bindings:

```cpp
template<typename InputContainer>
DataTypeVariant TransformPipeline::execute(InputContainer const& input) const {
    PipelineValueStore store;
    
    // 1. Run all reductions, populate store
    for (auto const& reduction : reductions_) {
        auto result = runReduction(reduction, input);
        store.set(reduction.output_key, result);
    }
    
    // 2. Execute transform steps with bound params
    for (auto const& step : steps_) {
        if (!step.param_bindings.empty()) {
            step.applyBindings(store);
        }
        // ... execute step ...
    }
}
```

### 2.3 Update JSON Schema and Loader

**File**: `src/DataManager/transforms/v2/core/PipelineLoader.hpp`

*Location: **core** - Public API for loading pipelines from JSON*

Update JSON parsing to support new format:

```json
{
  "name": "ZScoreNormalization",
  "reductions": [
    {"reduction": "Mean", "output_key": "computed_mean"},
    {"reduction": "StandardDeviation", "output_key": "computed_std"}
  ],
  "steps": [
    {
      "transform": "ScaleAndShift",
      "params": {
        "clamp_outliers": true
      },
      "bindings": {
        "mean": "computed_mean",
        "std_dev": "computed_std"
      }
    }
  ]
}
```

### 2.4 Deliverables

- [ ] `TransformPipeline::addReduction()` method
- [ ] Modified `execute()` with store population and binding
- [ ] Updated `PipelineLoader` JSON parsing
- [ ] Updated JSON schema documentation
- [ ] Integration tests for reduction → binding flow

---

## Phase 3: GatherResult Refactoring

**Goal**: Replace `TrialContext` injection with generic value store bindings.

### 3.1 Analyze Current GatherResult Context Usage

Current flow in `GatherResult`:

```cpp
// Current approach
TrialContext buildContext(size_type trial_idx) const {
    return TrialContext{
        .alignment_time = TimeFrameIndex(interval.start),
        .trial_index = orig_idx,
        .trial_duration = interval.end - interval.start,
        .end_time = TimeFrameIndex(interval.end)
    };
}

// Used by project(), reduce(), etc.
auto ctx = buildContext(i);
auto projection = factory(ctx);  // factory uses ContextInjectorRegistry
```

### 3.2 Create Generic Trial Store Builder

```cpp
class GatherResult {
public:
    /**
     * @brief Build value store for a specific trial
     * 
     * Creates a PipelineValueStore populated with standard trial values:
     * - "alignment_time": int64_t (trial start)
     * - "trial_index": int (trial number)
     * - "trial_duration": int64_t (end - start)
     * - "end_time": int64_t (trial end)
     * 
     * @param trial_idx Trial index
     * @return PipelineValueStore for this trial
     */
    [[nodiscard]] PipelineValueStore buildTrialStore(size_type trial_idx) const {
        PipelineValueStore store;
        auto interval = intervalAtReordered(trial_idx);
        auto orig_idx = originalIndex(trial_idx);
        
        store.set("alignment_time", static_cast<int64_t>(interval.start));
        store.set("trial_index", static_cast<int>(orig_idx));
        store.set("trial_duration", interval.end - interval.start);
        store.set("end_time", static_cast<int64_t>(interval.end));
        
        return store;
    }
};
```

### 3.3 Update Pipeline Binding Methods

**File**: `src/DataManager/transforms/v2/extension/ValueProjectionBinding.hpp`

*Location: **extension** - Type definitions for transform developers*

Replace context-aware factories with store-based factories:

```cpp
// NEW: Store-based projection factory
template<typename InElement, typename Value>
using ValueProjectionFactoryV2 = 
    std::function<ValueProjectionFn<InElement, Value>(PipelineValueStore const&)>;

// NEW: Bind pipeline to store-based factory
template<typename InElement, typename Value>
ValueProjectionFactoryV2<InElement, Value> bindValueProjectionV2(
    TransformPipeline const& pipeline);
```

### 3.4 Update GatherResult Methods

```cpp
template<typename Value>
[[nodiscard]] auto project(
    ValueProjectionFactoryV2<element_type, Value> const& factory) const {
    
    std::vector<ValueProjectionFn<element_type, Value>> projections;
    projections.reserve(size());
    
    for (size_type i = 0; i < size(); ++i) {
        auto store = buildTrialStore(i);
        projections.push_back(factory(store));
    }
    
    return projections;
}
```

### 3.5 Update NormalizeTimeParams

**File**: `src/DataManager/transforms/v2/algorithms/NormalizeTime/NormalizeTimeParams.hpp`

*Location: **algorithms** - Transform parameter types (living with their algorithm)*

Transform context-aware params to use bindings:

```cpp
// BEFORE (ContextAwareParams)
struct NormalizeTimeParams {
    rfl::Skip<std::optional<TimeFrameIndex>> alignment_time;
    
    void setContext(TrialContext const& ctx) {
        alignment_time = ctx.alignment_time;
    }
};

// AFTER (uses bindings in pipeline JSON)
struct NormalizeTimeParamsV2 {
    int64_t alignment_time = 0;  // Regular field, bound from store
};

// Pipeline JSON:
// {
//   "transform": "NormalizeTime",
//   "params": {},
//   "bindings": {"alignment_time": "alignment_time"}
// }
```

### 3.6 Deliverables

- [ ] `GatherResult::buildTrialStore()` method
- [ ] `ValueProjectionFactoryV2` type alias
- [ ] `bindValueProjectionV2()` function
- [ ] Updated `GatherResult::project()` and `reduce()`
- [ ] Migrated `NormalizeTimeParams` to binding-based approach
- [ ] Tests for GatherResult with new store pattern

---

## Phase 4: ZScore Migration

**Goal**: Migrate ZScoreNormalization from preprocessing to reduction-based approach.

### 4.1 Register Standard Reductions

**File**: `src/DataManager/transforms/v2/algorithms/StandardReductions/StandardReductions.hpp`

*Location: **algorithms** - Shared statistical reduction implementations*

```cpp
// Register common statistical reductions
namespace {
    // Mean reduction
    auto const reg_mean = RegisterRangeReduction<float, float, NoReductionParams>(
        "Mean",
        [](std::span<float const> values, NoReductionParams const&) -> float {
            if (values.empty()) return 0.0f;
            double sum = std::accumulate(values.begin(), values.end(), 0.0);
            return static_cast<float>(sum / values.size());
        });
    
    // Standard deviation reduction
    auto const reg_std = RegisterRangeReduction<float, float, NoReductionParams>(
        "StandardDeviation",
        [](std::span<float const> values, NoReductionParams const&) -> float {
            // Welford's algorithm implementation
        });
    
    // Min/Max reductions
    auto const reg_min = RegisterRangeReduction<float, float, NoReductionParams>("Min", ...);
    auto const reg_max = RegisterRangeReduction<float, float, NoReductionParams>("Max", ...);
}
```

### 4.2 Create ZScoreV2 Transform

**File**: `src/DataManager/transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.hpp`

*Location: **algorithms** - Algorithm-specific transform implementation*

```cpp
/**
 * @brief ZScore params without preprocessing
 * 
 * mean and std_dev are regular fields populated via param bindings
 * from reduction steps.
 */
struct ZScoreNormalizationParamsV2 {
    float mean = 0.0f;           // Bound from "computed_mean"
    float std_dev = 1.0f;        // Bound from "computed_std"
    bool clamp_outliers = false;
    float outlier_threshold = 3.0f;
    float epsilon = 1e-8f;
};

inline float zScoreNormalizationV2(float value, ZScoreNormalizationParamsV2 const& params) {
    float z = (value - params.mean) / (params.std_dev + params.epsilon);
    if (params.clamp_outliers) {
        z = std::clamp(z, -params.outlier_threshold, params.outlier_threshold);
    }
    return z;
}

// Registration
namespace {
    auto const reg_zscore_v2 = RegisterTransform<float, float, ZScoreNormalizationParamsV2>(
        "ZScoreNormalizeV2", zScoreNormalizationV2);
}
```

### 4.3 Example Pipeline JSON

```json
{
  "name": "ZScoreNormalization",
  "reductions": [
    {"reduction": "Mean", "output_key": "computed_mean"},
    {"reduction": "StandardDeviation", "output_key": "computed_std"}
  ],
  "steps": [
    {
      "transform": "ZScoreNormalizeV2",
      "params": {
        "clamp_outliers": true,
        "outlier_threshold": 3.0,
        "epsilon": 1e-8
      },
      "bindings": {
        "mean": "computed_mean",
        "std_dev": "computed_std"
      }
    }
  ]
}
```

### 4.4 Deliverables

- [ ] `StandardReductions.hpp` with Mean, StdDev, Min, Max
- [ ] `ZScoreNormalizationV2.hpp` (no preprocessing)
- [ ] Example pipeline JSON files
- [ ] Tests comparing V1 (preprocessing) vs V2 (reduction) behavior

---

## Phase 5: Deprecation and Cleanup

**Goal**: Deprecate old systems, remove unused code.

### 5.1 Deprecation Warnings

Add deprecation attributes to old interfaces:

```cpp
// ContextAwareParams.hpp
template<typename Params>
[[deprecated("Use param bindings with PipelineValueStore instead")]]
concept TrialContextAwareParams = ...;

// PreProcessingRegistry.hpp
class [[deprecated("Use range reductions with param bindings instead")]]
PreprocessingRegistry { ... };
```

### 5.2 Migration Guide

Create migration documentation:

**File**: `docs/developer/transforms/MIGRATION_TO_VALUE_STORE.md`

```markdown
# Migrating from Preprocessing/ContextInjection to Value Store

## Preprocessing Migration

### Before (V1)
```cpp
struct MyParams {
    rfl::Skip<float> cached_value;
    
    template<typename View>
    void preprocess(View view) {
        // Compute cached_value from view
    }
};
```

### After (V2)
```cpp
struct MyParams {
    float computed_value = 0.0f;  // Populated via binding
};

// Pipeline JSON
{
  "reductions": [{"reduction": "MyReduction", "output_key": "my_value"}],
  "steps": [{
    "transform": "MyTransform",
    "bindings": {"computed_value": "my_value"}
  }]
}
```

## Context Injection Migration

### Before
```cpp
struct MyParams {
    void setContext(TrialContext const& ctx) {
        alignment_time = ctx.alignment_time;
    }
};

auto factory = bindValueProjectionWithContext<In, Out>(pipeline);
auto result = gather_result.project(factory);
```

### After
```cpp
struct MyParams {
    int64_t alignment_time = 0;  // Regular field
};

// Pipeline with binding
{
  "bindings": {"alignment_time": "alignment_time"}
}

auto factory = bindValueProjectionV2<In, Out>(pipeline);
auto result = gather_result.project(factory);  // Uses buildTrialStore()
```
```

### 5.3 Remove Old Code

Once migration is complete:

1. Remove `PreProcessingRegistry.hpp`
2. Remove `tryAllRegisteredPreprocessing()` from `RegisteredTransforms.hpp`
3. Remove `ContextInjectorRegistry` class
4. Remove `TrialContext` struct
5. Update `RegisteredTransforms.hpp` to only include registration helpers

### 5.4 Deliverables

- [ ] Deprecation warnings added
- [ ] Migration guide documentation
- [ ] Removal of deprecated code (after grace period)
- [ ] Updated `RegisteredTransforms.hpp` (minimal includes)

---

## Phase 6: Testing and Validation

### 6.1 Unit Tests

| Component | Test File |
|-----------|-----------|
| PipelineValueStore | `tests/DataManager/TransformsV2/test_pipeline_value_store.test.cpp` |
| ParameterBinding | `tests/DataManager/TransformsV2/test_parameter_binding.test.cpp` |
| Reductions in Pipeline | `tests/DataManager/TransformsV2/test_pipeline_reductions.test.cpp` |
| GatherResult V2 | `tests/DataManager/utils/GatherResult_ValueStore.test.cpp` |
| ZScore V2 | `tests/DataManager/TransformsV2/test_zscore_v2.test.cpp` |

### 6.2 Benchmark Comparisons

Update benchmarks to compare:

- V1 preprocessing vs V2 reduction+binding for ZScore
- V1 ContextInjector vs V2 ValueStore for trial alignment
- Memory usage (header includes, binary size)
- Compile time impact

### 6.3 Integration Tests

- [ ] End-to-end pipeline from JSON with reductions and bindings
- [ ] GatherResult with sorted/reordered trials using V2 pattern
- [ ] UI widget loading pipeline with bindings

---

## Timeline

| Phase | Duration | Dependencies | Status |
|-------|----------|--------------|--------|
| Phase 1: Core Infrastructure | 1 week | None | ✅ COMPLETED |
| Phase 2: TransformPipeline Integration | 1 week | Phase 1 | ⏳ Next |
| Phase 3: GatherResult Refactoring | 1 week | Phase 2 | 📋 Planned |
| Phase 4: ZScore Migration | 3 days | Phase 2 | 📋 Planned |
| Phase 5: Deprecation/Cleanup | 1 week | Phases 3, 4 | 📋 Planned |
| Phase 6: Testing | Ongoing | All phases | 📋 Planned |

---

## Benefits Summary

| Metric | Before | After |
|--------|--------|-------|
| Header includes in RegisteredTransforms.hpp | All preprocessable params | Only registration helpers |
| Lines to add new preprocessable transform | ~20 (header + registration) | 0 (use JSON bindings) |
| Specialized context structs | TrialContext, (future: ComputedStatistics) | None (generic store) |
| Registries required | PreprocessingRegistry, ContextInjectorRegistry | None |
| JSON pipeline expressiveness | Implicit preprocessing | Explicit reduction + binding |
| Compile time impact | Full rebuild on any change | Incremental |

---

## Open Questions

1. **Backward Compatibility Period**: How long to keep V1 APIs before removal?
   - Suggested: 2 release cycles with deprecation warnings

2. **Hybrid Approach**: Allow both V1 preprocessing and V2 bindings temporarily?
   - Suggested: Yes, during transition period

3. **Store Key Namespacing**: Should trial stores have prefix (e.g., "trial.alignment_time")?
   - Suggested: No prefix, keep flat for simplicity

4. **Complex Bindings**: Support nested field binding (e.g., "params.nested.field")?
   - Suggested: Start with flat, add if needed

---

## References

- [reflect-cpp documentation](https://github.com/getml/reflect-cpp)
- [Current PreProcessingRegistry implementation](../../../src/DataManager/transforms/v2/extension/PreProcessingRegistry.hpp)
- [Current ContextAwareParams implementation](../../../src/DataManager/transforms/v2/extension/ContextAwareParams.hpp)
- [GatherResult implementation](../../../src/DataManager/utils/GatherResult.hpp)

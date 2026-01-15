# GatherResult + TransformPipeline Integration Design

## Overview

This document describes the design and implementation roadmap for integrating `GatherResult` with `TransformPipeline` to enable runtime-configurable, composable view transformations and reductions for trial-aligned analysis.

**Status:** Phases 1-6 Complete. Phase 7 Pending.

## Goals

1. **Runtime Composability**: Users can chain transforms at runtime (via UI or JSON)
2. **Automatic Discoverability**: All transforms and reductions are registry-registered
3. **Performance with Composability**: Lazy evaluation, no intermediate storage
4. **Contained Complexity**: All chaining infrastructure lives in `TransformPipeline`
5. **Minimal API Surface**: `GatherResult` has simple, focused methods

## Use Cases

### Primary: Raster Plot with Sorting

```
Input: DigitalEventSeries (spikes), DigitalIntervalSeries (trials)
       
User wants to:
1. Gather spikes by trial intervals
2. Normalize each trial's spike times to alignment (t=0 at trial start)
3. Compute first-spike latency per trial
4. Sort trials by latency
5. Draw raster (no intermediate storage)
```

### Secondary: Multi-Data Conditioning

```
Input: DigitalEventSeries (spikes), AnalogTimeSeries (behavior), 
       DigitalIntervalSeries (trials)

User wants to:
1. Gather both spikes and behavior by trial
2. Compute max behavior value per trial
3. Partition spikes into "high behavior" and "low behavior" groups
4. Draw with different colors
```

## Architecture

### Existing Reduction Infrastructure: TimeGroupedTransform

The transform system already has a reduction concept: `TimeGroupedTransform`. This operates on 
**all elements at a single time point** and is used for M→N transformations within the 
temporal structure of containers.

```cpp
// Existing concept in ElementTransform.hpp
template<typename F, typename In, typename Out, typename Params>
concept TimeGroupedTransform = requires(
    F f,
    std::span<In const> inputs,
    Params const& params) {
    { f(inputs, params) } -> std::convertible_to<std::vector<Out>>;
};

// Example: SumReduction
// At time T: [1.0, 2.0, 3.0] → [6.0]
// Used for: RaggedAnalogTimeSeries → AnalogTimeSeries
```

**This is NOT the same as what we need for GatherResult.** The distinction:

| Aspect | TimeGroupedTransform | RangeReduction (NEW) |
|--------|---------------------|----------------------|
| **Scope** | Elements at ONE time point | Elements across ALL time points |
| **Temporal Structure** | Preserved | Collapsed |
| **Signature** | `span<In> → vector<Out>` | `range<Element> → Scalar` |
| **Use Case** | Ragged → non-ragged containers | Trial → sort key |
| **Example** | Sum 3 mask areas at t=100 → one float | Count all spikes in trial → int |

### How They Compose

For GatherResult use cases, we may need BOTH:

```
Trial View (DigitalEventSeries)
    │
    ▼ [Element Transform: NormalizeTime]
    │
    ▼ lazy view of normalized EventWithId
    │
    ├──→ [Draw directly - no further reduction]
    │
    └──→ [RangeReduction: FirstPositiveLatency] → float (for sorting)
```

The element transforms (including existing TimeGroupedTransforms when operating on 
containers) handle the view-to-view transformations. The NEW range reductions 
handle the view-to-scalar collapse for sorting/partitioning.

### Component Responsibilities

| Component | Responsibility |
|-----------|----------------|
| `ElementRegistry` | Stores element transforms (existing) |
| `TimeGroupedTransform` | Per-time-point M→N reductions (existing) |
| `RangeReductionRegistry` | Stores whole-range → scalar reductions (NEW) |
| `TransformPipeline` | Composes transforms, produces view adaptors and reducers |
| `GatherResult` | Provides trial structure, consumes pipeline adaptors |

### Data Flow

```
                    ┌─────────────────────────────────────────────────┐
                    │              TransformPipeline                   │
                    │  ┌─────────┐   ┌─────────┐   ┌──────────────┐  │
                    │  │ Step 1  │ → │ Step 2  │ → │  Reduction   │  │
                    │  │(element)│   │(element)│   │  (terminal)  │  │
                    │  └─────────┘   └─────────┘   └──────────────┘  │
                    │                                                  │
                    │  bindToView<In,Out>() ──────→ ViewAdaptor       │
                    │  bindReducer<In,Scalar>() ──→ Reducer           │
                    └─────────────────────────────────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────────────────┐
│                           GatherResult                                │
│                                                                       │
│   Trial 0: view ──→ adaptor ──→ lazy_view ──→ [draw / reduce]        │
│   Trial 1: view ──→ adaptor ──→ lazy_view ──→ [draw / reduce]        │
│   Trial 2: view ──→ adaptor ──→ lazy_view ──→ [draw / reduce]        │
│   ...                                                                 │
└──────────────────────────────────────────────────────────────────────┘
```

## Implementation Phases

---

## Phase 1: RangeReductionRegistry Infrastructure ✅ COMPLETED

**Goal**: Create a registry for range reduction operations that consume entire ranges and produce scalars.

**Distinction from TimeGroupedTransform**: 
- `TimeGroupedTransform`: `span<In>` at ONE time → `vector<Out>` (preserves temporal structure)
- `RangeReduction`: `range<Element>` across ALL times → `Scalar` (collapses temporal structure)

### Step 1.1: Define Range Reduction Concepts and Types ✅

**File**: [src/DataManager/transforms/v2/core/RangeReductionTypes.hpp](../core/RangeReductionTypes.hpp)

Defines:
- `ParameterizedRangeReduction` concept: reduction function taking range and parameters
- `StatelessRangeReduction` concept: reduction without parameters
- `RangeReduction` concept: union of both patterns
- `RangeReductionMetadata` struct for discovery and serialization
- Helper type traits for concept checking

### Step 1.2: Implement RangeReductionRegistry ✅

**File**: [src/DataManager/transforms/v2/core/RangeReductionRegistry.hpp](../core/RangeReductionRegistry.hpp)

Implements:
- `RangeReductionRegistry` singleton with full type-erased infrastructure
- Type-safe registration: `registerReduction<Element, Scalar, Params>()`
- Stateless helper: `registerStatelessReduction<Element, Scalar>()`
- Discovery API: `getReductionNames()`, `getReductionsForInputType<T>()`, `getMetadata()`
- Type-safe execution: `execute<Element, Scalar, Params>(name, input, params)`
- Type-erased execution: `executeErased()` for pipeline use
- Parameter executor factory: `createParamExecutor()` with captured parameters
- JSON parameter deserialization: `createParamExecutorFromJson()`

### Step 1.3: Executor Pattern with Captured Parameters ✅

Implemented `TypedRangeReductionParamExecutor` that:
- Captures parameters at construction time
- Executes without per-call type dispatch
- Works correctly with both singleton and local registry instances (test isolation)

### Step 1.4: Tests for RangeReductionRegistry ✅

**File**: [tests/DataManager/TransformsV2/test_range_reduction_registry.test.cpp](../../../tests/DataManager/TransformsV2/test_range_reduction_registry.test.cpp)

Tests cover:
- Registration and lookup of stateless and parameterized reductions
- Type matching and metadata population
- Discovery API (by name, by input type, by output type)
- Type-safe execution with various reduction functions
- Type-erased execution for pipeline use
- Parameter executors with captured parameters
- JSON parameter factory for JSON-based pipelines
- Edge cases (empty ranges, NaN results, large inputs)
- All tests passing ✅

---

## Phase 2: Example Range Reductions ✅ COMPLETED

**Goal**: Implement common range reductions that demonstrate the pattern.

**Status**: Complete

**Note**: These are distinct from existing `TimeGroupedTransform` reductions like `SumReduction`.
Range reductions collapse an entire range to a scalar, while TimeGroupedTransforms
operate per-time-point within a container's temporal structure.

### Step 2.1: Event Range Reductions ✅

**File**: [src/DataManager/transforms/v2/algorithms/RangeReductions/EventRangeReductions.hpp](../algorithms/RangeReductions/EventRangeReductions.hpp)

| Reduction | Input | Output | Description |
|-----------|-------|--------|-------------|
| `EventCount` | `EventWithId` | `int` | Number of events |
| `FirstPositiveLatency` | `EventWithId` | `float` | Time of first event with t > 0 |
| `LastNegativeLatency` | `EventWithId` | `float` | Time of last event with t < 0 |
| `EventCountInWindow` | `EventWithId` | `int` | Events within time window (parameterized) |
| `MeanInterEventInterval` | `EventWithId` | `float` | Mean interval between events |
| `EventTimeSpan` | `EventWithId` | `float` | Duration from first to last event |

### Step 2.2: Value Range Reductions ✅

**File**: [src/DataManager/transforms/v2/algorithms/RangeReductions/ValueRangeReductions.hpp](../algorithms/RangeReductions/ValueRangeReductions.hpp)

| Reduction | Input | Output | Description |
|-----------|-------|--------|-------------|
| `MaxValue` | `TimeValuePoint` | `float` | Maximum value |
| `MinValue` | `TimeValuePoint` | `float` | Minimum value |
| `MeanValue` | `TimeValuePoint` | `float` | Mean value |
| `StdValue` | `TimeValuePoint` | `float` | Standard deviation |
| `TimeOfMax` | `TimeValuePoint` | `float` | Time at which max occurs |
| `TimeOfMin` | `TimeValuePoint` | `float` | Time at which min occurs |
| `TimeOfThresholdCross` | `TimeValuePoint` | `float` | First time value crosses threshold (parameterized) |
| `SumValue` | `TimeValuePoint` | `float` | Sum of all values |
| `ValueRange` | `TimeValuePoint` | `float` | Max - Min |
| `AreaUnderCurve` | `TimeValuePoint` | `float` | Trapezoidal integration |
| `CountAboveThreshold` | `TimeValuePoint` | `int` | Samples above threshold (parameterized) |
| `FractionAboveThreshold` | `TimeValuePoint` | `float` | Fraction above threshold (parameterized) |

### Step 2.3: Registration ✅

**File**: [src/DataManager/transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.cpp](../algorithms/RangeReductions/RegisteredRangeReductions.cpp)

All reductions registered with metadata for discoverability at static initialization time.

### Step 2.4: Tests ✅

**File**: [tests/DataManager/TransformsV2/test_range_reductions.test.cpp](../../../../tests/DataManager/TransformsV2/test_range_reductions.test.cpp)

- All reductions tested with known inputs
- Edge cases covered (empty range, no match, NaN results)
- Registry integration tests verify discovery API
- All tests passing ✅

---

## Phase 3: Context-Aware Parameters ✅ COMPLETED

**Goal**: Enable transforms to receive per-trial context (e.g., alignment time).

**Status**: Complete

### Step 3.1: Define Context Injection Convention ✅

**File**: [src/DataManager/transforms/v2/core/ContextAwareParams.hpp](core/ContextAwareParams.hpp)

Parameters that need context implement a `setContext` method:

```cpp
// Detection concept
template<typename Params, typename Context>
concept ContextAwareParams = requires(Params& p, Context const& ctx) {
    { p.setContext(ctx) };
};

// Convenience alias for trial context
template<typename Params>
concept TrialContextAwareParams = ContextAwareParams<Params, TrialContext>;
```

Also defines:
- `TrialContext` struct with alignment_time, trial_index, trial_duration, end_time
- `is_context_aware_params_v<Params, Context>` type trait
- `injectContext(params, ctx)` helper function (no-op for non-context-aware params)
- `hasRequiredContext(params)` helper to check if context has been set

### Step 3.2: Create NormalizeTime Transform ✅

**File**: [src/DataManager/transforms/v2/algorithms/Temporal/NormalizeTime.hpp](algorithms/Temporal/NormalizeTime.hpp)

Implements temporal normalization for trial-aligned analysis:

**Output Types**:
- `NormalizedEvent`: EventWithId with float normalized_time (satisfies TimeSeriesElement, EntityElement, ValueElement)
- `NormalizedValue`: TimeValuePoint with float normalized_time (satisfies TimeSeriesElement, ValueElement)

**Parameters**:
- `NormalizeTimeParams`: Context-aware params with `setContext(TrialContext)` and `setAlignmentTime(TimeFrameIndex)`

**Transform Functions**:

| Transform | Input | Output | Description |
|-----------|-------|--------|-------------|
| `normalizeEventTime` | `EventWithId` | `NormalizedEvent` | Normalize event time relative to alignment |
| `normalizeValueTime` | `TimeValuePoint` | `NormalizedValue` | Normalize sample time relative to alignment |

### Step 3.3: Register Temporal Transforms ✅

**File**: [src/DataManager/transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.cpp](algorithms/Temporal/RegisteredTemporalTransforms.cpp)

Registers with ElementRegistry:
- `NormalizeEventTime`: EventWithId → NormalizedEvent
- `NormalizeValueTime`: TimeValuePoint → NormalizedValue

Pipeline step factory registered for `NormalizeTimeParams`.

### Step 3.4: Tests ✅

**File**: [tests/DataManager/TransformsV2/test_context_aware_params.test.cpp](../../../tests/DataManager/TransformsV2/test_context_aware_params.test.cpp)

Tests cover:
- Concept detection (ContextAwareParams, TrialContextAwareParams)
- TrialContext construction and field access
- Context injection helpers (injectContext, hasRequiredContext)
- NormalizeTimeParams context injection and manual configuration
- NormalizedEvent construction, accessors, and concept satisfaction
- NormalizedValue construction, accessors, and concept satisfaction
- normalizeEventTime with positive/negative/zero offsets
- normalizeValueTime with value preservation
- Batch processing with entity ID preservation
- Error handling (throws when context not set)
- All tests passing ✅

---

## Phase 4: TransformPipeline View Adaptor API ✅ COMPLETED

**Goal**: Add methods to produce view adaptors and reducers from pipelines.

**Status**: Complete

### Step 4.1: Add Range Reduction Step Support ✅

**File**: [src/DataManager/transforms/v2/core/TransformPipeline.hpp](core/TransformPipeline.hpp)

Added to `TransformPipeline`:
- `setRangeReduction<IntermediateElement, Scalar, Params>()`: Set terminal range reduction
- `hasRangeReduction()`: Check if pipeline has terminal reduction
- `getRangeReduction()`: Get the reduction step (optional)
- `clearRangeReduction()`: Remove terminal reduction
- `isElementWiseOnly()`: Check if all steps are element-level

**File**: [src/DataManager/transforms/v2/core/ViewAdaptorTypes.hpp](core/ViewAdaptorTypes.hpp)

Defines type aliases and concepts:
- `ViewAdaptorFn<InElement, OutElement>`: Function type for view adaptation
- `ViewAdaptorFactory<InElement, OutElement>`: Factory that creates adaptors from context
- `ReducerFn<InElement, Scalar>`: Function type for range reduction
- `ReducerFactory<InElement, Scalar>`: Factory that creates reducers from context
- `RangeReductionStep`: Descriptor for terminal range reduction
- `BoundViewAdaptor`, `BoundReducer`: Result types with metadata

### Step 4.2: Implement `bindToView()` ✅

**File**: [src/DataManager/transforms/v2/core/TransformPipeline.hpp](core/TransformPipeline.hpp)

Free function template that:
- Takes a pipeline with element-level transforms
- Builds a composed transform function
- Returns `ViewAdaptorFn<InElement, OutElement>` that transforms span → vector
- Throws if pipeline is empty or contains time-grouped transforms

### Step 4.3: Implement `bindToViewWithContext()` ✅

**File**: [src/DataManager/transforms/v2/core/TransformPipeline.hpp](core/TransformPipeline.hpp)

Free function template that:
- Takes a pipeline with context-aware transforms
- Returns a factory function: `TrialContext → ViewAdaptorFn`
- Uses `ContextInjectorRegistry` for type-erased context injection
- Each factory invocation creates fresh adaptor with injected context

**File**: [src/DataManager/transforms/v2/core/ContextAwareParams.hpp](core/ContextAwareParams.hpp)

Added `ContextInjectorRegistry`:
- Singleton registry for type-erased context injection
- `registerInjector<Params>()`: Register injector for context-aware params
- `tryInject(std::any&, TrialContext)`: Inject context at runtime
- `RegisterContextInjector<Params>`: Static registration helper

### Step 4.4: Implement `bindReducer()` ✅

**File**: [src/DataManager/transforms/v2/core/TransformPipeline.hpp](core/TransformPipeline.hpp)

Free function template that:
- Takes a pipeline with `setRangeReduction()` configured
- Combines element transforms with terminal range reduction
- Returns `ReducerFn<InElement, Scalar>` that transforms span → scalar
- Throws if pipeline has no range reduction

### Step 4.5: Implement `bindReducerWithContext()` ✅

**File**: [src/DataManager/transforms/v2/core/TransformPipeline.hpp](core/TransformPipeline.hpp)

Free function template that:
- Takes a pipeline with context-aware transforms and range reduction
- Returns a factory function: `TrialContext → ReducerFn`
- Injects context before each reduction

### Step 4.6: Tests ✅

**File**: [tests/DataManager/TransformsV2/test_pipeline_adaptors.test.cpp](../../../../tests/DataManager/TransformsV2/test_pipeline_adaptors.test.cpp)

Tests cover:
- ViewAdaptorTypes type definitions and signatures
- RangeReductionStep construction and field access
- TransformPipeline setRangeReduction/hasRangeReduction/clearRangeReduction
- TransformPipeline isElementWiseOnly detection
- ContextInjectorRegistry registration and injection
- bindToView() basic functionality with NormalizeEventTime
- bindToViewWithContext() with multiple trials and alignments
- bindReducer() throws without range reduction
- bindReducerWithContext() throws without range reduction
- Integration test: Raster plot workflow with multiple trials
- All tests passing ✅

---

## Phase 5: GatherResult Integration (REVISED)

**Goal**: Add methods to GatherResult that consume pipeline adaptors using lazy value projections.

**Status**: ✅ Complete

### Design Revision: Value Projection Instead of New Types

**Problem Identified**: Phase 3 introduced `NormalizedEvent` which doesn't fit in `ElementVariant` 
and would cause type explosion as more transforms are added.

**Solution**: Use **lazy value projections** instead of intermediate element types.

#### Key Insight: Extract Fundamental Type, Process in Pipeline, Retain Identity at Call Site

Instead of passing composite types like `EventWithId` through the pipeline, extract
the fundamental value (`TimeFrameIndex`) and pass that instead:

```
Source Element (EventWithId)
    │
    ├── .id() ─────────────────────────────────────────────────→ EntityId (retained at caller)
    │
    └── .time() ─→ TimeFrameIndex ──→ [Pipeline: NormalizeTime] ──→ float
```

This avoids creating intermediate types and keeps transforms focused on fundamental values.
The consumer extracts the `.time()` component and uses it in the pipeline:

```cpp
auto projection_fn = pipeline.bindValueProjection<TimeFrameIndex, float>(ctx);

for (auto const& event : trial_view) {
    TimeFrameIndex time = event.time();       // Extract fundamental type
    float norm_time = projection_fn(time);    // Process in pipeline
    EntityId id = event.id();                 // Retain identity from source
    draw_point(norm_time, trial_row, id);
}
```

**Why this approach**:
- `TimeFrameIndex` is now a first-class member of `ElementVariant` and `BatchVariant`
- Transforms work on fundamental types, reducing type explosion
- Caller explicitly manages the relationship between extracted value and source identity
- Cleaner separation of concerns: pipeline handles temporal values, caller handles entity tracking

#### Why This Is Still Runtime Composable

| Aspect | Runtime Composability |
|--------|----------------------|
| Pipeline configuration | ✅ JSON/UI selects transforms by name |
| Transform chaining | ✅ `A → B → C` composed at runtime |
| Context injection | ✅ TrialContext injected per-trial |
| Type erasure | ✅ `std::function<float(EventWithId const&)>` |

The only compile-time requirement is knowing the **input element type** (from the 
container type) and the **final scalar type** (from the reduction or projection).

### Step 5.1: Define Value Projection Types ✅

**File**: `src/DataManager/transforms/v2/core/ValueProjectionTypes.hpp`

Implemented:
- `ValueProjectionFn<InElement, Value>`: Function type for element → value projection
- `ValueProjectionFactory<InElement, Value>`: Factory that creates projections from TrialContext
- `ErasedValueProjectionFn` / `ErasedValueProjectionFactory`: Type-erased versions for runtime composition
- `makeProjectedView()`: Lazy view yielding (element_ref, value) pairs
- `makeValueView()`: Lazy view yielding only projected values
- `eraseValueProjection()` / `eraseValueProjectionFactory()`: Type erasure helpers
- `recoverValueProjection()` / `recoverValueProjectionFactory()`: Type recovery helpers
- `ValueProjection` / `ValueProjectionFactoryConcept`: Concepts for validation

**Tests**: `tests/DataManager/TransformsV2/test_value_projection_types.test.cpp` ✅

### Step 5.2: Add TimeFrameIndex to ElementVariant and Update Transforms ✅

**File**: `src/DataManager/transforms/v2/core/TransformTypes.hpp`

Added `TimeFrameIndex` as a fundamental type in both variant aliases:

```cpp
using ElementVariant = std::variant<float, TimeFrameIndex, Point2D<float>, Line2D, Mask2D>;
using BatchVariant = std::variant<
    std::vector<float>,
    std::vector<TimeFrameIndex>,
    std::vector<Point2D<float>>,
    std::vector<Line2D>,
    std::vector<Mask2D>
>;
```

**Note**: `float` is placed first to ensure `ElementVariant{}` can be default-constructed in pipeline code.

**File**: `src/DataManager/transforms/v2/algorithms/Temporal/NormalizeTime.hpp`

Implemented value projection functions that accept `TimeFrameIndex` and output `float`:

```cpp
// Value projection version (operates on fundamental type):
[[nodiscard]] inline float normalizeTimeValue(
        TimeFrameIndex const& time,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    return static_cast<float>(time.getValue() - alignment.getValue());
}

[[nodiscard]] inline float normalizeSampleTimeValue(
        AnalogTimeSeries::TimeValuePoint const& sample,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    return static_cast<float>(sample.time().getValue() - alignment.getValue());
}
```

Registered as:
- `NormalizeTimeValue`: `TimeFrameIndex → float`
- `NormalizeSampleTimeValue`: `TimeValuePoint → float`

The original `NormalizedEvent` and `NormalizedValue` types are no longer used, as the
value projection approach (fundamental type → float) is simpler and more flexible.

**File**: `src/DataManager/transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.cpp` ✅

### Step 5.3: Implement `bindValueProjection()` ✅

**File**: `src/DataManager/transforms/v2/core/TransformPipeline.hpp`

Implemented two template functions for binding pipelines to value projections:

```cpp
/**
 * @brief Bind a pipeline to produce a value projection function
 * 
 * Creates a function that transforms a single input element into a scalar value
 * by applying all pipeline steps.
 */
template<typename InElement, typename Value>
ValueProjectionFn<InElement, Value> bindValueProjection(TransformPipeline const & pipeline);

/**
 * @brief Bind a pipeline with context-aware transforms to produce a value projection factory
 * 
 * Creates a factory function that takes TrialContext and returns a value projection.
 */
template<typename InElement, typename Value>
ValueProjectionFactory<InElement, Value> bindValueProjectionWithContext(TransformPipeline const & pipeline);
```

**Key Implementation Details:**
- Builds composition chain of element transforms using `ElementRegistry::executeWithDynamicParams`
- Captures metadata (types, params) by value to avoid dangling references
- For context-aware version, injects `TrialContext` into parameters before creating projections
- Returns `Value` type directly from `ElementVariant` using `std::get<Value>`
- Supports all element transforms in the pipeline (rejects time-grouped transforms)

**Tests**: Covered by existing test suite ✅

### Step 5.4: Add GatherResult `project()` Method ✅

**File**: `src/DataManager/utils/GatherResult.hpp`

Implemented `project()` method that:

```cpp
/**
 * @brief Project values across all trials using a pipeline
 * 
 * Returns a lazy view adaptor that, when applied to each trial's view,
 * yields (element_ref, projected_value) pairs.
 * 
 * @tparam Value The projected value type
 * @param factory Context-aware projection factory
 * @return Object that can be iterated to get projected views per trial
 * 
 * @example
 * @code
 * auto projection_factory = bindValueProjectionWithContext<EventWithId, float>(pipeline);
 * 
 * for (size_t i = 0; i < result.size(); ++i) {
 *     auto ctx = result.buildContext(i);
 *     auto projection = projection_factory(ctx);
 *     
 *     for (auto const& event : (*result[i])->view()) {
 *         float norm_time = projection(event);
 *         EntityId id = event.id();
 *         // Draw using norm_time and id
 *     }
 * }
 * @endcode
 */
template<typename Value>
auto project(ValueProjectionFactory<element_type, Value> const& factory) const;
```

### Step 5.5: Add GatherResult Helper Methods ✅

Implemented in `src/DataManager/utils/GatherResult.hpp`:

- `buildContext(trial_idx)`: Creates TrialContext with alignment_time, trial_index, duration, end_time
- `reduce(reducer_factory)`: Applies reducer to all trials, returns vector of scalars
- `sortIndicesBy(reducer_factory, ascending)`: Sorts trials by reduction result, handles NaN values
- `reorder(indices)`: Creates new GatherResult with trials in specified order
- `originalIndex(reordered_idx)`: Maps reordered position to original trial index
- `isReordered()`: Check if result has been reordered
- `intervalAtReordered(idx)`: Gets interval accounting for reordering

### Step 5.6: Define element_type Alias ✅

Added to `src/DataManager/utils/GatherResult.hpp`:

```cpp
/// Element type yielded by view iteration
using element_type = WhiskerToolbox::Gather::element_type_of_t<T>;
```

Implemented `element_type_of<T>` trait with specializations for:
- `DigitalEventSeries` → `EventWithId`
- `AnalogTimeSeries` → `AnalogTimeSeries::TimeValuePoint`
- `DigitalIntervalSeries` → `IntervalWithId`

### Step 5.7: Implementation Details ✅

All implementations completed with proper handling for:
- Reordered results (buildContext uses intervalAtReordered)
- NaN values in sorting (sort to end)
- Empty trials in reductions
- Bounds checking with appropriate exceptions

### Step 5.8: Tests ✅

**File**: `tests/DataManager/utils/GatherResult_Pipeline.test.cpp`

All tests implemented and passing:
- `element_type` alias verification for all container types
- `buildContext()` produces correct TrialContext for each trial
- `project()` creates per-trial projections with correct alignment
- `reduce()` with event count and first-spike latency
- `reduce()` handles empty trials correctly
- `sortIndicesBy()` ascending and descending order
- `sortIndicesBy()` handles NaN values
- `reorder()` creates correctly ordered result
- `reorder()` tracks original indices
- `reorder()` validation (wrong size, out-of-range)
- `intervalAtReordered()` returns correct interval
- Full raster plot workflow integration test

### Step 5.9: Remove Deprecated Types and Refactor to TimeFrameIndex ✅

**Status**: Complete

The following types and functions have been completely removed since the refined
approach uses fundamental types (`TimeFrameIndex`) instead of composite types:

**Removed Types**:
- `NormalizedEvent` struct (no longer needed)
- `NormalizedValue` struct (no longer needed)

**Removed Functions**:
- `normalizeEventTime(EventWithId)` - replaced by `normalizeTimeValue(TimeFrameIndex)`
- `normalizeValueTime(TimeValuePoint)` - kept as-is for direct use

**Removed Registry Entries**:
- `NormalizeEventTime` transform registration
- `NormalizeValueTime` transform registration

**Refactored Pattern**:
- Old: `EventWithId → NormalizedEvent` (creates new composite type)
- New: `TimeFrameIndex → float` (operates on extracted fundamental value)

**Updated Tests**:
- `test_context_aware_params.test.cpp` - updated to extract `.time()` before calling transforms
- `test_pipeline_adaptors.test.cpp` - refactored to use `TimeFrameIndex` directly

**Key Insight**: By adding `TimeFrameIndex` to `ElementVariant`, we avoid type explosion
and enable cleaner transform signatures. Callers explicitly manage the extraction of
fundamental types from composite elements and the retention of identity information.

---

## Phase 6: JSON Serialization ✅ COMPLETED

**Goal**: Enable saving/loading pipelines with reductions.

**Status**: Complete

### Step 6.1: Extend PipelineDescriptor ✅

**File**: [src/DataManager/transforms/v2/core/PipelineLoader.hpp](../core/PipelineLoader.hpp)

Added:
- `RangeReductionStepDescriptor` struct with `reduction_name`, optional `parameters`, and optional `description`
- Optional `range_reduction` field to `PipelineDescriptor`
- Updated documentation with JSON examples showing pipelines with and without range reductions

### Step 6.2: Update PipelineLoader ✅

**File**: [src/DataManager/transforms/v2/core/PipelineLoader.hpp](../core/PipelineLoader.hpp)
**File**: [src/DataManager/transforms/v2/core/PipelineLoader.cpp](../core/PipelineLoader.cpp)

Implemented:
- `loadRangeReductionFromDescriptor()`: Loads range reduction from JSON descriptor
- `RangeReductionRegistry::deserializeParameters()`: Deserializes parameters using registered deserializers
- Updated `loadPipelineFromJson()` to handle optional range reduction field
- Updated validation to accept pipelines with range reduction even if no steps present
- Added `TransformPipeline::setRangeReductionErased()`: Type-erased method for JSON loading

Enhanced **RangeReductionRegistry**:
- Added `param_deserializers_` map for JSON parameter deserialization
- Updated `registerParamHandling()` to register deserializers
- Added `deserializeParameters()` method for registry-based parameter loading

### Step 6.3: Tests ✅

**File**: [tests/unit/DataManager/transforms/test_pipeline_loader.test.cpp](../../../../tests/unit/DataManager/transforms/test_pipeline_loader.test.cpp)

Tests added:
- `RangeReductionStepDescriptor can be serialized to JSON` - Basic serialization
- `RangeReductionStepDescriptor can be deserialized from JSON` - Basic deserialization
- `loadRangeReductionFromDescriptor loads valid reduction with no parameters` - Stateless reductions
- `loadRangeReductionFromDescriptor loads valid reduction with parameters` - Parameterized reductions
- `loadRangeReductionFromDescriptor rejects unknown reduction` - Error handling
- `loadRangeReductionFromDescriptor rejects invalid parameters` - Validation
- `loadPipelineFromJson loads pipeline without range reduction` - Backward compatibility
- `loadPipelineFromJson loads pipeline with range reduction` - New feature
- `loadPipelineFromJson loads multi-step pipeline with range reduction` - Complex pipelines
- `loadPipelineFromJson rejects pipeline with invalid range reduction` - Error handling
- `Pipeline descriptor round-trips through JSON with range reduction` - Round-trip serialization
- `Full pipeline with reduction can be loaded and used` - Integration test

All tests passing ✅

---

## Phase 7: Documentation and Examples

**Goal**: Complete user-facing documentation and working examples.

**Status**: Not started

### Step 7.1: Update DESIGN.md

Add section on view adaptors and range reductions (pending)

### Step 7.2: Add Example Code

**File**: `src/DataManager/transforms/v2/examples/RasterPlotExample.cpp` (pending)

Complete working example of the raster plot use case.

### Step 7.3: Update User Documentation

Add to `docs/user_guide/` explaining core concepts and workflows (pending)

---

## Implementation Order Summary

| Phase | Status | Effort | Dependencies | Priority |
|-------|--------|--------|--------------|----------|
| 1. RangeReductionRegistry | ✅ Complete | 2-3 days | None | Critical |
| 2. Example Range Reductions | ✅ Complete | 1-2 days | Phase 1 | Critical |
| 3. Context-Aware Params | ✅ Complete | 1-2 days | None | Critical |
| 4. Pipeline Adaptor API | ✅ Complete | 3-4 days | Phases 1-3 | Critical |
| 5. GatherResult Integration | ✅ Complete | 2-3 days | Phase 4 | Critical |
| 6. JSON Serialization | ✅ Complete | 1-2 days | Phases 1-5 | High |
| 7. Documentation | Not Started | 1-2 days | All | Medium |

**Completed Effort**: 11-16 days (Phases 1-6)  
**Remaining Estimated Effort**: 1-2 days

**Status**: Phase 6 Complete. Ready to proceed with Phase 7 (Documentation).

---

## Testing Strategy

### Unit Tests

- Each range reduction individually
- Context injection mechanism
- View adaptor correctness
- Reducer correctness

### Integration Tests

- Full pipeline: gather → transform → reduce → sort
- JSON round-trip with execution
- Cross-data sorting

### Performance Tests

- Verify no intermediate allocations in lazy paths
- Benchmark against manual loop implementation

---

## Open Questions

1. ~~**Should `reorder()` create new intervals or reuse source?**~~
   - ✅ RESOLVED: Reordered result stores `_reorder_indices` vector
   - Views are reordered, intervals are accessed via `originalIndex()` mapping
   - `buildContext()` and `intervalAtReordered()` account for reordering

2. ~~**How to handle TimeFrame in reordered GatherResult?**~~
   - ✅ RESOLVED: Views reference source TimeFrame unchanged
   - Reordering is logical only (view order), not temporal

3. **Should range reductions support cancellation like element transforms?**
   - Could add `ComputeContext` parameter
   - Defer to future if not immediately needed

4. ~~**Type deduction for template parameters?**~~
   - ✅ RESOLVED: Value projections require explicit `<InElement, Value>` which is acceptable
   - Input element type known from container, output value type known from pipeline terminal

5. **Relationship between RangeReduction and TimeGroupedTransform?**
   - They are distinct concepts with different purposes
   - RangeReduction: `range<Element>` → `Scalar` (collapses all time)
   - TimeGroupedTransform: `span<Element>` at time T → `vector<Element>` at time T
   - Both are "reductions" but operate at different scopes

6. ~~**Intermediate types like NormalizedEvent in ElementVariant?**~~
   - ✅ RESOLVED: Use value projections instead of intermediate element types
   - Value projection: `EventWithId → float` (just the normalized time)
   - EntityId obtained from source element via `.id()` accessor
   - Avoids type explosion in ElementVariant
   - Maintains runtime composability

7. **What about transforms that genuinely need to modify multiple fields?**
   - Example: Transform that filters AND normalizes
   - Could use tuple projection: `EventWithId → std::tuple<float, bool>`
   - Or chain projections: filter view → normalize projection
   - Defer to implementation phase

---

## Appendix: TimeFrameIndex in ElementVariant - Design Rationale

The implementation adds `TimeFrameIndex` as a first-class member of `ElementVariant` and
`BatchVariant`. This represents a refined understanding of how transforms should work
with time-series data.

### Original Design vs Refined Design

**Original Approach (Phase 3)**:
- Create composite types (`NormalizedEvent`, `NormalizedValue`) that carry both value and metadata
- Put these types in `ElementVariant`
- Problem: Type explosion as more transforms are added; `ElementVariant` becomes unwieldy

**Refined Approach (Phase 5)**:
- Extract fundamental types (`TimeFrameIndex`) from composite types before pipeline
- Add fundamental types to `ElementVariant` directly
- Process fundamental types in transforms
- Caller manages relationship between extracted value and original identity
- Result: Simpler variant, cleaner signatures, explicit responsibility boundaries

### Benefits

| Aspect | Original | Refined |
|--------|----------|---------|
| **Transform Signature** | `EventWithId → NormalizedEvent` | `TimeFrameIndex → float` |
| **ElementVariant Size** | Grows with each new output type | Fixed (fundamental types only) |
| **Type Complexity** | Creates many intermediate types | Minimizes intermediate types |
| **Caller Responsibility** | Implicit (identity in output type) | Explicit (retain identity from source) |
| **Composability** | Transforms can't easily chain | Clear composition rules |
| **Performance** | May require materializing composites | Lazy evaluation of fundamentals |

### Implementation Pattern

When using transforms with identity-bearing types:

```cpp
// Source: EventWithId has both .time() and .id()
auto event = /* ... */;

// Step 1: Extract fundamental type
TimeFrameIndex time = event.time();
EntityId id = event.id();

// Step 2: Process fundamental type through pipeline
float norm_time = normalizeTimeValue(time, params);

// Step 3: Use result with retained identity
draw_point(norm_time, trial_row, id);
```

This pattern scales well: as new transforms are added, they operate on fundamental
types without requiring new entries in `ElementVariant`.

---

## Appendix: Value Projection vs Element Transform

The Phase 5 revision introduces **value projections** as distinct from **element transforms**.

### Comparison

| Aspect | Element Transform | Value Projection |
|--------|-------------------|------------------|
| **Signature** | `Element → Element'` | `Element → Value` |
| **Output** | New element type | Scalar or tuple |
| **Identity** | Carried in output type | From source element |
| **Storage** | Can materialize containers | Computed on-demand |
| **Use Case** | Container transforms | View analysis |
| **Registry** | `ElementRegistry` | Same registry, different binding |

### When to Use Each

**Element Transform** (existing):
- Transforming container data types: `MaskData → LineData`
- Output will be stored in DataManager
- New element type needed (e.g., `Mask2D → Line2D`)

**Value Projection** (new):
- Trial-aligned analysis: `EventWithId → float`
- Output consumed immediately (drawing, reduction)
- Source element provides identity info

### How Value Projections Use the Same Registry

Value projections are **element transforms** under the hood - they're just registered
with scalar output types and bound differently:

```cpp
// Registration (same as any element transform):
registry.registerElement<EventWithId, float, NormalizeTimeParams>(
    "NormalizeEventTimeValue",
    normalizeEventTimeValue,
    {...}
);

// Binding for container transforms:
auto adaptor = bindToView<EventWithId, float>(pipeline);  // span<In> → vector<Out>

// Binding for value projection:
auto projection = bindValueProjection<EventWithId, float>(pipeline);  // In → Out (single element)
```

The difference is in **how the bound function is used**, not in how transforms are registered.

---

## Appendix: Reduction Concept Comparison

| Aspect | TimeGroupedTransform | RangeReduction |
|--------|---------------------|----------------|
| **Location** | `ElementTransform.hpp` | `RangeReductionTypes.hpp` (NEW) |
| **Registry** | `ElementRegistry` | `RangeReductionRegistry` (NEW) |
| **Input** | `std::span<In const>` | `std::ranges::input_range` |
| **Output** | `std::vector<Out>` | `Scalar` |
| **Scope** | All elements at ONE time point | All elements across ALL times |
| **Temporal** | Preserved | Collapsed |
| **Use Case** | `RaggedAnalogTimeSeries` → `AnalogTimeSeries` | `Trial view` → `sort key` |
| **Example** | `SumReduction`: [1,2,3] at t=100 → [6] at t=100 | `EventCount`: trial with 50 events → 50 |

---

## Appendix: Key Type Relationships

```
GatherResult<DigitalEventSeries>
    └── views: vector<shared_ptr<DigitalEventSeries>>
              └── view() → range<EventWithId>
                          └── EventWithId
                              ├── .time() → TimeFrameIndex
                              ├── .id() → EntityId
                              └── .value() → TimeFrameIndex

GatherResult<AnalogTimeSeries>
    └── views: vector<shared_ptr<AnalogTimeSeries>>
              └── elements() → range<TimeValuePoint>
                              └── TimeValuePoint
                                  ├── .time() → TimeFrameIndex
                                  └── .value() → float
```

---

## Appendix: Concepts Used

```cpp
// Element with time accessor
template<typename T>
concept TimeSeriesElement = requires(T const& t) {
    { t.time() } -> std::convertible_to<TimeFrameIndex>;
};

// Element with entity ID
template<typename T>
concept EntityElement = TimeSeriesElement<T> && requires(T const& t) {
    { t.id() } -> std::convertible_to<EntityId>;
};

// Element with value accessor
template<typename T>
concept ValueElement = TimeSeriesElement<T> && requires(T const& t) {
    { t.value() };
};

// Context-injectable parameters
template<typename Params, typename Context>
concept ContextAwareParams = requires(Params& p, Context ctx) {
    { p.setContext(ctx) };
};

// Range reduction (NEW - distinct from TimeGroupedTransform)
template<typename F, typename Element, typename Scalar, typename Params>
concept RangeReduction = requires(
    F f,
    std::ranges::input_range auto&& range,
    Params const& params) {
    { f(std::forward<decltype(range)>(range), params) } -> std::convertible_to<Scalar>;
};
```

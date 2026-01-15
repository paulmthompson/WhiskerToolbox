# GatherResult + TransformPipeline Integration Design

## Overview

This document describes the design and implementation roadmap for integrating `GatherResult` with `TransformPipeline` to enable runtime-configurable, composable view transformations and reductions for trial-aligned analysis.

**Status:** Phase 1 (RangeReductionRegistry) Complete. Phases 2-7 Pending.

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

## Phase 2: Example Range Reductions

**Goal**: Implement common range reductions that demonstrate the pattern.

**Status**: Not started

**Note**: These are distinct from existing `TimeGroupedTransform` reductions like `SumReduction`.
Range reductions collapse an entire range to a scalar, while TimeGroupedTransforms
operate per-time-point within a container's temporal structure.

### Step 2.1: Event Range Reductions

**File**: `src/DataManager/transforms/v2/algorithms/RangeReductions/EventRangeReductions.hpp` (pending)

| Reduction | Input | Output | Description |
|-----------|-------|--------|-------------|
| `FirstPositiveLatency` | `EventWithId` | `float` | Time of first event with t > 0 |
| `LastNegativeLatency` | `EventWithId` | `float` | Time of last event with t < 0 |
| `EventCount` | `EventWithId` | `int` | Number of events |
| `EventCountInWindow` | `EventWithId` | `int` | Events within time window |

### Step 2.2: Value Range Reductions

**File**: `src/DataManager/transforms/v2/algorithms/RangeReductions/ValueRangeReductions.hpp` (pending)

| Reduction | Input | Output | Description |
|-----------|-------|--------|-------------|
| `MaxValue` | `ValueElement` | `float` | Maximum value |
| `MinValue` | `ValueElement` | `float` | Minimum value |
| `MeanValue` | `ValueElement` | `float` | Mean value |
| `TimeOfMax` | `ValueElement` | `float` | Time at which max occurs |
| `TimeOfMin` | `ValueElement` | `float` | Time at which min occurs |
| `TimeOfThresholdCross` | `ValueElement` | `float` | First time value crosses threshold |

### Step 2.3: Registration

**File**: `src/DataManager/transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.cpp` (pending)

Register all reductions with metadata for discoverability.

### Step 2.4: Tests

**File**: `tests/DataManager/TransformsV2/test_range_reductions.test.cpp` (pending)

- Test each reduction with known inputs
- Test edge cases (empty range, no match, etc.)

---

## Phase 3: Context-Aware Parameters

**Goal**: Enable transforms to receive per-trial context (e.g., alignment time).

**Status**: Not started

### Step 3.1: Define Context Injection Convention

Parameters that need context implement a `setContext` method:

```cpp
// Detection trait
template<typename Params, typename Context>
concept ContextAwareParams = requires(Params& p, Context ctx) {
    { p.setContext(ctx) };
};
```

### Step 3.2: Implement Context Injection in PipelineStep

**File**: Modify `src/DataManager/transforms/v2/core/TransformPipeline.hpp` (pending)

Add context injection mechanism to `PipelineStep`.

### Step 3.3: Create NormalizeTime Transform

**File**: `src/DataManager/transforms/v2/algorithms/Temporal/NormalizeTime.hpp` (pending)

Implement temporal normalization for event and analog time series elements.

### Step 3.4: Register Temporal Transforms

### Step 3.5: Tests

- Test context injection works
- Test normalization produces correct offsets

---

## Phase 4: TransformPipeline View Adaptor API

**Goal**: Add methods to produce view adaptors and reducers from pipelines.

**Status**: Not started

### Step 4.1: Add Range Reduction Step Support

Modify `TransformPipeline` to support a terminal range reduction (pending)

### Step 4.2: Implement `bindToView()`

Create view adaptor from element transforms (pending)

### Step 4.3: Implement `bindToViewWithContext()`

Create context-aware view adaptor factory (pending)

### Step 4.4: Implement `bindReducer()`

Create reducer from pipeline with terminal range reduction (pending)

### Step 4.5: Implement `bindReducerWithContext()`

Context-aware reducer factory (pending)

### Step 4.6: Tests

**File**: `tests/DataManager/TransformsV2/test_pipeline_adaptors.test.cpp` (pending)

- Test `bindToView()` produces correct lazy view
- Test `bindToViewWithContext()` with alignment
- Test `bindReducer()` produces correct scalar
- Test composition with multiple steps
- Test with different element types

---

## Phase 5: GatherResult Integration

**Goal**: Add methods to GatherResult that consume pipeline adaptors.

**Status**: Not started

### Step 5.1: Add `applyPipeline()` Method

Apply pipeline to all trials, returning lazy transformed views (pending)

### Step 5.2: Add `reducePipeline()` Method

Apply pipeline with reduction to all trials (pending)

### Step 5.3: Add `sortBy()` Method

Sort trials by pipeline reduction result (pending)

### Step 5.4: Add `partitionBy()` Method

Partition trials by threshold on reduction result (pending)

### Step 5.5: Add Cross-Data Sorting Support

Reorder GatherResult by indices (pending)

### Step 5.6: Tests

**File**: `tests/DataManager/utils/test_gather_result_pipeline.test.cpp` (pending)

- Test `applyPipeline()` with normalize transform
- Test `reducePipeline()` with count, latency
- Test `sortBy()` produces correct order
- Test `partitionBy()` produces correct split
- Test cross-data sorting workflow

---

## Phase 6: JSON Serialization

**Goal**: Enable saving/loading pipelines with reductions.

**Status**: Not started

### Step 6.1: Extend PipelineDescriptor

Add optional terminal range reduction field (pending)

### Step 6.2: Update PipelineLoader

Handle the optional range reduction field in JSON (pending)

### Step 6.3: Tests

- Test loading pipeline with range reduction
- Test loading pipeline without range reduction
- Test round-trip serialization

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
| 2. Example Range Reductions | Not Started | 1-2 days | Phase 1 | Critical |
| 3. Context-Aware Params | Not Started | 1-2 days | None | Critical |
| 4. Pipeline Adaptor API | Not Started | 3-4 days | Phases 1-3 | Critical |
| 5. GatherResult Integration | Not Started | 2-3 days | Phase 4 | Critical |
| 6. JSON Serialization | Not Started | 1-2 days | Phases 1-5 | High |
| 7. Documentation | Not Started | 1-2 days | All | Medium |

**Completed Effort**: 2-3 days (Phase 1)  
**Remaining Estimated Effort**: 9-15 days

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

1. **Should `reorder()` create new intervals or reuse source?**
   - Current design assumes views reference source intervals
   - May need to store interval order separately

2. **How to handle TimeFrame in reordered GatherResult?**
   - Views reference source TimeFrame
   - Reordering doesn't change time bases

3. **Should range reductions support cancellation like element transforms?**
   - Could add `ComputeContext` parameter
   - Defer to future if not immediately needed

4. **Type deduction for template parameters?**
   - Currently requires explicit `<InElement, OutElement>` 
   - Could add convenience overloads that deduce from container type

5. **Relationship between RangeReduction and TimeGroupedTransform?**
   - They are distinct concepts with different purposes
   - RangeReduction: `range<Element>` → `Scalar` (collapses all time)
   - TimeGroupedTransform: `span<Element>` at time T → `vector<Element>` at time T
   - Both are "reductions" but operate at different scopes

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

# Transformation System V2 Architecture

## Overview

This document describes the redesigned transformation architecture that addresses limitations in the current system and provides a unified, composable approach to data transformations.

## Current Limitations

### Problems with Existing System

1. **Code Duplication**: Similar transformation patterns exist in:
   - `transforms/` (container-level transformations)
   - `utils/TableView/computers/` (row-based computations)
   - No shared abstraction layer

2. **No Lazy Evaluation**: All transformations materialize intermediate results
   - Example: MaskData → skeletonize → MaskData → fit line → LineData
   - Each step processes all data before the next begins
   - High memory usage and cache-unfriendly

3. **Limited Composability**: 
   - EventInIntervalComputer has enum for (Presence, Count, Gather, Gather_Center)
   - These could be separate transforms: Gather → Count/Presence/Center
   - Reduces code duplication and improves reusability

4. **Boilerplate Parameter Handling**: 
   - Manual JSON serialization/deserialization
   - Error-prone parameter validation
   - reflect-cpp can eliminate most of this

5. **No Transform Provenance**: 
   - Can't track which Mask2D produced which Line2D
   - EntityID relationships not captured

## Architecture Goals

### Core Principles

1. **Separation of Concerns**:
   - **Element transforms**: Operate on single data items (Mask2D, Line2D, float)
   - **Container transforms**: Automatically lifted from element transforms
   - **Storage strategy**: Independent of data type semantics

2. **Lazy Evaluation Where Possible**:
   - Range-based pipelines using C++20 ranges
   - Fusion of transform chains at compile time
   - Materialize only when needed

3. **Compile-Time Dispatch with Runtime Configuration**:
   - Users configure pipelines at runtime (UI, JSON)
   - Factory dispatches to compile-time optimized implementations
   - Type erasure only at boundaries

4. **Composability**:
   - Small, focused transforms
   - Chain multiple transforms
   - Automatic batching/fusion when beneficial

5. **Provenance Tracking**:
   - EntityID relationships through transform chains
   - Query parent/child relationships
   - Useful for debugging and visualization

## System Components

### 1. Element-Level Transforms

**Concept**: Transforms that operate on individual data elements.

```cpp
// Pure element transform signature
template<typename In, typename Out, typename Params>
using ElementTransformFunc = std::function<Out(In const&, Params const&)>;

// Examples:
Mask2D skeletonize(Mask2D const& mask, SkeletonParams const& params);
Line2D maskToLine(Mask2D const& mask, MaskToLineParams const& params);
float lineAngle(Line2D const& line, AngleParams const& params);
```

**Properties**:
- Stateless (or stateful with explicit context)
- Type-safe at compile time
- Composable via function composition
- Testable in isolation

### 2. Range-Based Pipelines

**Concept**: Apply transforms lazily using C++20 ranges.

```cpp
// Instead of:
MaskData masks1 = skeletonize(masks_input);  // Materialize
LineData lines = maskToLine(masks1);          // Materialize
AnalogTimeSeries angles = lineAngles(lines); // Materialize

// Use:
auto pipeline = MaskDataView(masks_input)
    | ranges::transform(skeletonize)
    | ranges::transform(maskToLine)
    | ranges::transform(lineAngle);

// Only materializes when consumed:
auto angles = pipeline | ranges::to<AnalogTimeSeries>();
```

**Implementation Note**: The container types (MaskData, LineData, etc.) inherit from 
`std::ranges::view_interface` and provide their own range views via `elements()` method. 
No separate `RangeAdaptors` wrapper is needed - the standard library ranges already 
work directly with our containers.

**Benefits**:
- No intermediate allocations
- Cache-friendly (process one element through entire chain)
- Compiler can optimize/fuse operations
- Lazy evaluation (compute on demand)

### 3. Storage Abstraction

**Current State**:
- AnalogTimeSeries has AnalogDataStorage (vector vs memory-mapped)
- Other types (MaskData, LineData) don't have storage abstraction

**Proposed**:

```cpp
template<typename T>
class DataStorage {
    virtual T getAt(size_t index) const = 0;
    virtual size_t size() const = 0;
};

// Implementations:
template<typename T>
class VectorStorage : public DataStorage<T> { ... };

template<typename T>
class LazyTransformStorage : public DataStorage<T> {
    // Stores transform chain + input reference
    // Computes on-demand when getAt() called
};

template<typename T>
class MemoryMappedStorage : public DataStorage<T> { ... };
```

**Benefit**: AnalogTimeSeries computed via lazy evaluation can store the pipeline instead of materialized values.

### 4. Container-Level Automatic Lifting

**Concept**: Automatically generate container transforms from element transforms.

```cpp
// Register element transform
registry.registerElementTransform("Skeletonize", skeletonize_element);

// Automatically available at container level:
std::shared_ptr<MaskData> skeletonized = 
    registry.executeContainer<MaskData, MaskData>(
        "Skeletonize", masks, params);
```

**Implementation**:
- Use template metaprogramming to map element types to container types
- Trait: `ContainerFor<Mask2D>` → `MaskData`
- Automatically generate container version using range views

### 5. Runtime Configuration with Compile-Time Execution

**User Workflow**:
1. User selects transforms via UI (dropdown, drag-drop, etc.)
2. Configures parameters via generated forms
3. Pipeline descriptor created (JSON-serializable)
4. Factory dispatches to compile-time typed implementation
5. Execution uses fully optimized pipeline

**Example**:

```cpp
// Runtime configuration (from UI)
PipelineDescriptor desc {
    .name = "Whisker Angle Analysis",
    .input_type = "MaskData",
    .output_type = "AnalogTimeSeries",
    .steps = {
        {"Skeletonize", params1},
        {"MaskToLine", params2},
        {"LineAngle", params3}
    }
};

// Factory creates typed pipeline
auto pipeline = factory.createPipeline(desc);
// Type: CompiledPipeline<MaskData, AnalogTimeSeries>

// Execute with full compile-time optimization
auto result = pipeline->execute(mask_data);
```

### 6. Provenance Tracking

**Concept**: Track EntityID relationships through transforms.

```cpp
// When LazyTransformStorage is used:
auto line_data = masks_to_lines(mask_data);  // Lazy

// Query provenance:
EntityId line_id = line_data.getEntityIdAt(t, i);
std::vector<EntityId> parent_masks = 
    line_data.getParentEntityIds(line_id);

// Works even for derived data:
auto angles = lineAngles(line_data);  // AnalogTimeSeries
EntityId angle_id = ...;
std::vector<EntityId> source_lines = 
    angles.getParentEntityIds(angle_id);
std::vector<EntityId> source_masks = 
    angles.getAncestorEntityIds(angle_id, EntityKind::MaskEntity);
```

**Implementation**:
- Each LazyTransformStorage tracks parent EntityIDs
- EntityRegistry stores transform graph
- Query methods walk the graph

## Migration Strategy

### Phase 1: Parallel Implementation (Current)

1. Create `src/DataManager/transforms/v2/` directory
2. Implement core infrastructure:
   - ElementTransformRegistry
   - RangeTransformPipeline
   - PipelineFactory
   - reflect-cpp parameter integration
3. Port 2-3 example transforms (MaskArea, MaskToLine, LineAngle)
4. Add comprehensive tests

### Phase 2: Integration

1. Both old and new systems accessible via UI
2. New transforms registered in both registries
3. Users can test new system alongside old
4. Gather feedback, iterate

### Phase 3: Migration

1. Gradually port transforms from v1 to v2
2. Update UI to use new system by default
3. Deprecate old system
4. Remove old implementation once all transforms migrated

### Phase 4: Advanced Features

1. Implement LazyTransformStorage
2. Add provenance tracking
3. Extend to TableView system
4. Performance optimization based on profiling

## Implementation Files

### Core Infrastructure

- `ElementTransform.hpp` - Element-level transform interfaces and concepts
- `ElementRegistry.hpp` - Registry for element transforms
- `TransformPipeline.hpp` - Lazy range-based pipeline builder
- `StorageStrategy.hpp` - Storage abstraction interfaces
- `LazyStorage.hpp` - Lazy evaluation storage implementation
- `ContainerTraits.hpp/.cpp` - Mapping between element and container types
- `CompileTimeRegistry.hpp` - Compile-time typed transform registry
- `RuntimePipeline.hpp` - Type-erased runtime pipeline configuration
- `PipelineFactory.hpp` - Factory for creating optimized pipelines
- `ProvenanceTracker.hpp` - EntityID relationship tracking

**Note**: Container types (MaskData, LineData, etc.) provide range views directly via 
inheritance from `std::ranges::view_interface`. No separate range adapter layer is needed.

### Parameter Handling

- `TransformParameters.hpp` - reflect-cpp based parameter definitions
- `ParameterValidation.hpp` - Compile-time parameter validation
- `ParameterSerialization.hpp` - JSON serialization via reflect-cpp

### Example Transforms

- `examples/MaskAreaTransform.hpp` - Element-level mask area
- `examples/MaskToLineTransform.hpp` - Mask to line fitting
- `examples/LineAngleTransform.hpp` - Line angle computation
- `examples/ComposedTransform.hpp` - Example of transform composition

### Testing

- `ElementTransform.test.cpp` - Element transform tests
- `RangePipeline.test.cpp` - Pipeline composition tests
- `LazyEvaluation.test.cpp` - Lazy evaluation correctness
- `Provenance.test.cpp` - EntityID tracking tests
- `Performance.benchmark.cpp` - Performance comparison

## Key Design Decisions

### Why Ranges Instead of Virtual Functions?

**Option A: Virtual function call per element**
```cpp
class ITransform {
    virtual std::any execute(std::any const& input) = 0;
};

for (auto& mask : masks) {
    result.push_back(transform->execute(mask));  // Virtual call
}
```

**Option B: Range-based with type knowledge**
```cpp
template<typename Func>
auto transform(Range auto&& range, Func func) {
    return range | std::views::transform(func);  // Inlined
}
```

**Decision**: Ranges (Option B)
- Zero overhead when type is known
- Compiler can fuse multiple transforms
- Cache-friendly (element processed through full pipeline)
- Still supports runtime configuration via type erasure at boundaries

### Why Both Element and Container Registries?

**Rationale**:
- Some transforms naturally operate on containers (e.g., batch normalization)
- Most transforms are element-level and can be automatically lifted
- Keep flexibility for both patterns
- Container transforms can be optimized differently (parallelization, etc.)

### When to Materialize vs Keep Lazy?

**Always Materialize**:
- User explicitly saves to DataManager
- Transform requires multiple passes over data
- Data needed for multiple downstream consumers

**Keep Lazy**:
- Intermediate pipeline steps
- Single-pass consumption
- Memory constraints
- Interactive visualization (compute visible range only)

**User Control**:
```cpp
// Keep lazy (until consumed)
auto pipeline = masks | skeletonize | maskToLine;

// Explicitly materialize
auto lines = pipeline.materialize();

// Or let DataManager decide
data_manager->addData("lines", pipeline);  // May keep lazy
```

## Performance Considerations

### Compile-Time Optimization

The range-based approach allows the compiler to:
1. Inline all transform functions
2. Fuse loops (single pass through data)
3. Apply SIMD vectorization
4. Eliminate temporary allocations

### Memory Efficiency

**Old approach**:
```
Input: 1000 masks @ 1MB each = 1000 MB
Step 1: Skeletonize → 1000 MB (temp)
Step 2: Fit lines → 100 MB (temp)  
Step 3: Calculate angles → 8 KB
Peak memory: ~2100 MB
```

**New approach (lazy)**:
```
Input: 1000 masks @ 1MB each = 1000 MB
Pipeline: Process one mask at a time
Peak memory: ~1001 MB (1 temp mask)
```

### Parallelization

Range-based approach is compatible with parallel execution:

```cpp
// Sequential
auto result = masks | skeletonize | maskToLine | collect;

// Parallel (when appropriate)
auto result = masks 
    | ranges::views::chunk(100)  // Process in batches
    | ranges::views::transform(parallel_process)
    | ranges::views::join
    | collect;
```

## Examples

See `examples/` directory for complete working examples of:
- Simple element transform
- Multi-step pipeline
- Lazy evaluation
- Runtime configuration
- Provenance tracking
- TableView integration

## References

- C++20 Ranges: https://en.cppreference.com/w/cpp/ranges
- reflect-cpp: https://github.com/getml/reflect-cpp
- CRTP Pattern: https://en.cppreference.com/w/cpp/language/crtp
- Type Erasure: https://www.modernescpp.com/index.php/c-core-guidelines-type-erasure-with-templates

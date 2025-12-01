# Transformation System V2 Architecture

## Overview

This document describes the redesigned transformation architecture that addresses limitations in the current system and provides a unified, composable approach to data transformations.

**Status:** Phase 1 & 2 complete (core infrastructure operational). This document describes both implemented features and future architectural goals.

## Current Implementation Status

### ✅ Implemented (Phase 1, 2 & 3)
- **ElementRegistry**: Compile-time typed transform registry
- **TypedParamExecutor**: Eliminates per-element parameter casts and type dispatch
- **TransformPipeline**: Multi-step pipeline with execution fusion
- **View-Based Pipelines**: Lazy evaluation with `executeAsView()` (zero intermediate allocations)
- **Container Automatic Lifting**: Element transforms automatically work on containers
- **Native Container Transforms**: Direct implementation of container-to-container transforms (e.g. for temporal operations)
- **ContainerTraits**: Type mapping system (Mask2D ↔ MaskData, etc.)
- **Advanced Features**: Ragged outputs, multi-input transforms (binary via tuple), time-grouped transforms
- **reflect-cpp Integration**: Parameter serialization with automatic validation
- **JSON Pipeline Loading**: PipelineDescriptor, PipelineLoader with full validation
- **Fuzz Testing**: Comprehensive corpus and fuzz tests for pipeline loading
- **Examples**: MaskArea, SumReduction, and LineMinPointDist (binary) transforms with tests

### 🔄 Planned (Phase 4+)
- **UI Integration**: Pipeline Builder Widget
- **Provenance Tracking**: EntityID relationship tracking
- **Lazy Storage**: On-demand computation for derived data
- **Parallel Execution**: Multi-threaded transform execution

See `NEXT_STEPS.md` for detailed roadmap and recommendations.

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

### 0. Type Traits Layer (✅ Implemented)

**New Foundation Layer**: As of recent refactoring, fundamental type properties have been extracted from the transformation system into a dedicated type traits layer at `src/DataManager/TypeTraits/`.

**Rationale**: Type traits (raggedness, temporal properties, entity tracking) are fundamental properties of data types that should be usable beyond just the transformation system. By placing them at the DataManager level, they can be leveraged by:
- Transform system (current primary user)
- Generic algorithms across DataManager
- UI widgets (adapting display based on container properties)
- Serialization systems
- Future features requiring type introspection

**Design**: Intrusive traits - each container type defines its own `DataTraits` nested struct:

```cpp
class MaskData : public RaggedTimeSeries<Mask2D> {
public:
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<MaskData, Mask2D> {
        static constexpr bool is_ragged = true;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = true;
        static constexpr bool is_spatial = true;
    };
    // ...
};
```

**Usage**:
```cpp
// Query traits
using namespace WhiskerToolbox::TypeTraits;
static_assert(is_ragged_v<MaskData>);
using ElementType = element_type_t<MaskData>;  // Mask2D

// Use concepts
template<RaggedContainer T>
void processRaggedData(T const& container) { /* ... */ }
```

**Location**: `src/DataManager/TypeTraits/DataTypeTraits.hpp`  
**Documentation**: `src/DataManager/TypeTraits/README.md`

### 1. Element-Level Transforms (✅ Implemented)

**Concept**: Transforms that operate on individual data elements.

```cpp
// Element transform signature (using std::function)
template<typename In, typename Out, typename Params>
using ElementTransformFunc = std::function<Out(In const&, Params const&)>;

// Examples:
float calculateMaskArea(Mask2D const& mask, MaskAreaParams const& params);
float sumValues(float const& value, SumReductionParams const& params);
```

**Properties**:
- Stateless (parameters passed explicitly)
- Type-safe at compile time
- Composable via TransformPipeline
- Testable in isolation

**Implementation:** See `core/ElementTransform.hpp` and `core/ElementRegistry.hpp`

### 2. Pipeline Fusion & View-Based Execution (✅ Implemented)

**Current Implementation**: Multi-step pipelines with two execution modes:

**Mode A: Fused Execution (Eager)**
Executes all steps in a single pass, materializing the final result immediately.

```cpp
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<SkeletonParams>("Skeletonize", p1);
pipeline.addStep<MaskAreaParams>("CalculateArea", p2);
auto areas = pipeline.execute(masks);  // Single pass, no intermediate MaskData
```

**Mode B: View-Based Execution (Lazy)**
Returns a lazy view that computes elements on-demand. Zero intermediate allocations.

```cpp
// Returns lazy view of (TimeFrameIndex, DataTypeVariant) pairs
auto view = pipeline.executeAsView(masks);

// Or typed view
auto view_typed = pipeline.executeAsViewTyped<MaskData, float>(masks);

// Compose with std::views
auto filtered = view_typed 
    | std::views::filter([](auto p) { return p.second > 10.0f; });

// Materialize only when needed
auto result = std::make_shared<RaggedAnalogTimeSeries>(filtered);
```

**Mode C: Dynamic Output (Single Template)**
Allows specifying only the input type, with the output type being a variant. This simplifies runtime dispatch by reducing the combinatorial explosion of template instantiations.

```cpp
// Pipeline with known input but dynamic output
TransformPipeline<MaskData> pipeline; 
pipeline.addStep<SkeletonParams>("Skeletonize", p1);
pipeline.addStep<MaskAreaParams>("CalculateArea", p2);

// Returns DataTypeVariant
DataTypeVariant result = pipeline.execute(masks);
```

**Benefits Achieved**:
- Single iteration over input data
- No intermediate container allocations
- Better cache locality
- Support for infinite/streaming data (via views)

**Implementation:** See `core/TransformPipeline.hpp` and `core/ContainerTransform.hpp`
### 3. Storage Abstraction (🔄 Planned)

**Current State**:
- Container types store data directly (vector-based)
- All transforms materialize their output
- AnalogTimeSeries has some storage abstraction (AnalogDataStorage)

**Proposed Future Enhancement**:

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
```

**Note**: Current eager fusion approach already minimizes intermediate allocations. Lazy storage may not provide significant additional benefit given time-indexed nature of data.

### 4. Container-Level Transforms (✅ Implemented)

**Concept**: Transforms that operate on entire containers.

**Type A: Automatic Lifting (Element → Container)**
Automatically generate container transforms from element transforms.

```cpp
// Register element transform
registry.registerTransform<Mask2D, float, MaskAreaParams>(
    "CalculateMaskArea", calculateMaskArea);

// Automatically available at container level:
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries, MaskAreaParams>(
    "CalculateMaskArea", masks, params);
```

**Type B: Native Container Transforms**
Directly implemented container-to-container transforms. Essential for:
- Temporal operations (e.g., thresholding with lockout time)
- Global statistics (e.g., normalization using global mean)
- Operations requiring full context

```cpp
// Register native container transform
registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, ThresholdParams>(
    "AnalogEventThreshold", analogEventThreshold);
```

**How it works**:
- **Lifting**: ContainerTraits maps element types to container types; metaprogramming generates iteration code.
- **Native**: Registry stores and executes container-level functions directly.

**Implementation:** See `core/ContainerTraits.hpp`, `core/ContainerTransform.hpp`, and `core/ElementRegistry.hpp`

### 5. Runtime Configuration with Compile-Time Execution (🔄 Planned)

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

### 6. Provenance Tracking (🔄 Planned)

**Concept**: Track EntityID relationships through transforms.

**Proposed Future API**:
```cpp
// Query provenance:
EntityId line_id = line_data.getEntityIdAt(t, i);
std::vector<EntityId> parent_masks = 
    line_data.getParentEntityIds(line_id);

// Multi-level queries:
auto angles = lineAngles(line_data);
EntityId angle_id = ...;
std::vector<EntityId> source_masks = 
    angles.getAncestorEntityIds(angle_id, EntityKind::MaskEntity);
```

**Implementation Plan**:
- Store parent EntityIDs in transformed data
- EntityRegistry maintains transform graph
- Query methods walk the graph
- Integrate with existing EntityID system

**Use Cases**:
- Debugging ("which mask produced this line?")
- Data lineage visualization
- Complex multi-step analysis tracking

**Note**: Not critical for initial deployment. Can be added as opt-in feature after core system is stable.

## Implementation Status

### Phase 1 & 2: Core Infrastructure ✅ COMPLETE

**Completed:**
- ✅ `ElementTransform.hpp` - Transform function wrappers
- ✅ `ElementRegistry.hpp` - Compile-time typed registry with TypedParamExecutor
- ✅ `TransformPipeline.hpp` - Multi-step pipeline with fusion
- ✅ `ContainerTraits.hpp/.cpp` - Element ↔ Container type mappings
- ✅ `ContainerTransform.hpp` - Container lifting utilities
- ✅ `algorithms/MaskArea/MaskArea.hpp` - Working example transform
- ✅ `algorithms/SumReduction/SumReduction.hpp` - Working example transform
- ✅ `algorithms/LineMinPointDist/LineMinPointDist.hpp` - Binary transform example
- ✅ `core/RegisteredTransforms.hpp` - Registration code with binary support
- ✅ `tests/DataManager/TransformsV2/test_*.test.cpp` - Comprehensive unit tests

### Phase 3a: Parameter Serialization & Pipeline Loading ✅ COMPLETE

**Completed:**
- ✅ `core/ParameterIO.hpp` - reflect-cpp integration with validation
- ✅ `core/PipelineLoader.hpp` - JSON pipeline schema and loading
- ✅ `tests/DataManager/TransformsV2/test_parameter_io.test.cpp` - Parameter I/O tests
- ✅ `tests/DataManager/TransformsV2/test_pipeline_loader.test.cpp` - Pipeline loading tests (25+ cases)
- ✅ `tests/fuzz/unit/DataManager/transforms/fuzz_v2_parameter_io.cpp` - Parameter fuzz tests
- ✅ `tests/fuzz/unit/DataManager/transforms/fuzz_v2_pipeline_loader.cpp` - Pipeline fuzz tests (7 functions)
- ✅ `tests/fuzz/corpus/v2_parameters/` - Parameter test corpus (8 files)
- ✅ `tests/fuzz/corpus/v2_pipelines/` - Pipeline test corpus (10 files)

### Phase 3: Runtime Configuration 🔄 IN PROGRESS

**Completed:**
- ✅ `core/ParameterIO.hpp` - reflect-cpp parameter serialization with validation
- ✅ `core/PipelineLoader.hpp` - JSON pipeline schema (PipelineDescriptor, PipelineStepDescriptor)
- ✅ `algorithms/LineMinPointDist/LineMinPointDist.hpp` - Binary transform example (multi-input via tuple)
- ✅ Comprehensive unit tests and fuzz tests with corpus

**To Be Implemented:**
- ⏳ `runtime/PipelineFactory.hpp` - Runtime type dispatch factory (type-erased interface)
- ⏳ `runtime/PipelineExecutor.hpp` - Type-erased execution interface for UI integration
- ⏳ Migration of existing V1 transforms to V2 with JSON support

### Phase 4: Advanced Features 🔄 FUTURE

**Deferred (evaluate need after Phase 3):**
- ⏳ `storage/LazyStorage.hpp` - On-demand computation storage
- ⏳ `provenance/ProvenanceTracker.hpp` - EntityID relationship tracking
- ⏳ C++20 ranges integration - True lazy pipelines
- ⏳ Parallel execution support

**Rationale for Deferral:**
- Current fusion approach provides most benefits
- Time-indexed data complicates lazy evaluation
- Need real-world usage data before optimizing further

See `NEXT_STEPS.md` for detailed roadmap.

## Migration Strategy

### Phase 1 & 2: Parallel Implementation ✅ COMPLETE

### Phase 1 & 2: Parallel Implementation ✅ COMPLETE

1. ✅ Created `src/DataManager/transforms/v2/` directory
2. ✅ Implemented core infrastructure:
   - ElementRegistry with TypedParamExecutor system
   - TransformPipeline with execution fusion
   - ContainerTraits for automatic lifting
3. ✅ Ported 2 example transforms (MaskArea, SumReduction)
4. ✅ Added basic tests

**Key Achievement:** TypedParamExecutor eliminates per-element parameter casts and type dispatch overhead.

### Phase 3: Runtime Configuration 🔄 NEXT

1. Integrate reflect-cpp for parameter serialization
2. Define JSON schema for pipeline descriptors
3. Implement PipelineFactory for runtime type dispatch
4. Add pipeline save/load functionality
5. Add comprehensive tests

**Goal:** Enable users to build, save, and load analysis pipelines via UI.

**Estimated Effort:** 9-13 days

### Phase 4: UI Integration 🔄 FUTURE

1. Add V2 transforms to DataTransform_Widget
2. Create pipeline builder UI
3. Auto-generate parameter forms from metadata
4. Add pipeline visualization
5. Add execution progress reporting

**Goal:** Make V2 transforms accessible to all users.

**Estimated Effort:** 2-3 weeks

### Phase 5: Migration 🔄 FUTURE

1. Migrate transforms from V1 to V2 gradually
2. Gather user feedback and iterate
3. Deprecate V1 system
4. Remove V1 after full migration

**Goal:** Complete transition to V2 system.

## Key Architectural Decisions

### Why Ranges Instead of Virtual Functions?

**Option A: Virtual function call per element**
```cpp
class ITransform {
    virtual DataTypeVariant execute(DataTypeVariant const& input) = 0;
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

### Current Implementation (✅ Fusion-Based)

**Memory Efficiency with Pipeline Fusion:**
```
Input: 1000 masks @ 1MB each = 1000 MB
Pipeline: MaskData → process → AnalogTimeSeries (fused execution)
Peak memory: ~1001 MB (input + 1 intermediate element + output)
```

**Benefits Achieved:**
- No intermediate container allocations between steps
- Single pass through input data
- Better cache locality (element processed through full pipeline)
- TypedParamExecutor eliminates per-element overhead (~20-30ns per element saved)

### Future with True Lazy Evaluation (🔄 Deferred)

**Potential with C++20 Ranges:**
```
Input: 1000 masks @ 1MB each = 1000 MB
Lazy Pipeline: Process one mask at a time through entire chain
Peak memory: ~1001 MB (input + 1 temp mask)
```

**Note:** Current fusion approach already achieves most benefits. True lazy ranges deferred pending:
- Real-world memory profiling
- User demand for on-demand computation
- Evidence that time-indexed data benefits from laziness

### Parallelization (🔄 Future)

Future parallel execution strategy:
```cpp
// Mark expensive transforms in metadata
meta.is_expensive = true;

// Thread pool will parallelize across time frames
auto result = pipeline.execute(mask_data);  // Automatic parallelization
```

## Current Examples

See `algorithms/` directory for working code:
- `MaskAreaTransform.hpp` - Simple element transform (Mask2D → float)
- `SumReductionTransform.hpp` - Reduction transform (float → float)
- `LineMinPointDistTransform.hpp` - Binary transform ((Line2D, Point2D) → float)
- `RegisteredTransforms.hpp` - Registration and typed executor factories
- `ParameterIO.hpp` - reflect-cpp parameter serialization with validation
- `PipelineLoader.hpp` - JSON pipeline loading with comprehensive validation

## Next Steps

See `NEXT_STEPS.md` for detailed roadmap and implementation recommendations.

**Immediate Priorities:**
1. Expand test coverage (multi-step pipelines, ragged outputs, etc.)
2. Migrate 3-5 real transforms from V1 to validate system
3. Implement runtime configuration (JSON pipelines, reflect-cpp parameters)
4. UI integration (pipeline builder, parameter forms)

## References

- reflect-cpp: https://github.com/getml/reflect-cpp
- C++20 Ranges: https://en.cppreference.com/w/cpp/ranges
- Type Erasure: https://www.modernescpp.com/index.php/c-core-guidelines-type-erasure-with-templates


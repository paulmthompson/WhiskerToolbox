# Transforms V2 - Next Steps and Recommendations

## Current Status Summary

**✅ Phase 1, 2 & 3 Complete** - Core infrastructure, View Pipelines, and JSON configuration fully operational:

- **ElementRegistry**: Compile-time typed transform registry with TypedParamExecutor system
- **TypedParamExecutor**: Eliminates per-element parameter casts and type dispatch overhead
- **TransformPipeline**: Multi-step pipeline with execution fusion
- **View-Based Pipelines**: Lazy evaluation with `executeAsView()` (zero intermediate allocations)
- **Container Automatic Lifting**: Element transforms automatically work on containers
- **Binary Transforms**: Multi-input support via std::tuple with BinaryElementTransform
- **Advanced Features**: Ragged outputs, multi-input transforms (binary), time-grouped transforms
- **reflect-cpp Integration**: Automatic parameter serialization with validation
- **JSON Pipeline Loading**: Complete schema with PipelineDescriptor and comprehensive validation
- **Comprehensive Testing**: 50+ unit tests, 12+ fuzz tests, 18+ corpus files
- **Working Examples**: MaskArea, SumReduction, LineMinPointDist (binary) transforms with full tests
- **Provenance Tracking**: Full lineage tracking with `LineageRegistry` and `EntityResolver`
- **DataManager Integration**: `load_data_from_json_config_v2()` with V1-compatible JSON format

**Performance Characteristics:**
- Parameter cast: Once per pipeline (not per element)
- Type dispatch: Once per pipeline (not per element)
- Per-element overhead: ~2 virtual calls + 2 variant visits (input/output only)
- Fusion: Multiple transforms execute in single pass (no intermediate containers)
- View Mode: Pull-based execution, zero intermediate allocations, compatible with std::views

## Recommended Next Steps

### Priority 1: UI Integration (High Impact, High Complexity)

**Why:** Make V2 transforms accessible to users; enable visual pipeline building

**Tasks:**
1. Add V2 transforms to DataTransform_Widget dropdown
2. Create pipeline builder UI (drag-drop transform nodes)
3. Auto-generate parameter forms from metadata
4. Add pipeline save/load functionality
5. Add pipeline visualization (DAG view)
6. Add execution progress reporting

**Estimated Effort:** 2-3 weeks

### Priority 2: Migrate 3-5 Real Transforms from V1 (High Impact, Medium Complexity)

**Why:** Validate the system with real-world use cases; identify missing features

**Recommended Candidates:**
1. **Skeletonize** (Mask2D → Mask2D) - Image processing
2. **Centroid** (Mask2D → Point2D) - Geometry extraction
3. **MaskToLine** (Mask2D → Line2D) - Curve fitting
4. **LineAngle** (Line2D → float) - Geometric measurement
5. **Normalize** (AnalogTimeSeries → AnalogTimeSeries) - Statistics

**For each transform:**
1. Extract element-level function from V1 operation class
2. Define parameter struct (plain struct, no reflect-cpp yet)
3. Register in `RegisteredTransforms.hpp`
4. Add unit tests
5. Keep V1 version for comparison

**Estimated Effort:** 3-5 days (depending on complexity)

### Priority 3: Advanced Features (Lower Priority, Various Complexity)

#### 3a. Parallel Execution (Medium Impact, High Complexity)

**Why:** Utilize multi-core CPUs for expensive transforms

**Implementation:**
- Mark expensive transforms in metadata (`is_expensive = true`)
- Use thread pool for parallel element processing
- Add chunking strategy for load balancing
- Handle thread-local state for transforms

**Estimated Effort:** 2-3 weeks

**Note:** Many transforms are already fast; profile before implementing

#### 3b. Lazy Storage Strategy (Low-Medium Impact, High Complexity)

**Why:** Avoid materializing intermediate results for derived data

**Implementation:**
- Abstract storage behind `DataStorage<T>` interface
- Implement `LazyTransformStorage` that stores pipeline + input reference
- Compute on-demand when `getAt(index)` called
- Add caching strategy for frequently accessed elements

**Estimated Effort:** 2-3 weeks

**Note:** Complexity of time-indexed data with variable frame counts makes this challenging

## Recommended Roadmap

### ✅ Sprint 1, 2 & 3 Complete (Completed)
- ✅ **Priority 1:** Comprehensive testing (50+ unit tests, 12+ fuzz tests)
- ✅ **Priority 3a:** Parameter serialization with reflect-cpp
- ✅ **Priority 3b:** Pipeline JSON schema and loading
- ✅ **View Pipelines:** Lazy evaluation infrastructure
- ✅ **Provenance Tracking:** Lineage system implemented
- ✅ **DataManager Integration:** `load_data_from_json_config_v2()` with V1-compatible format

### Sprint 4 (Current - 1-2 weeks)
- **Priority 1:** UI Integration (Pipeline Builder)
- **Priority 2:** Migrate 3-5 real transforms from V1
- **Goal:** Validate system with real-world transforms and enable UI access

### Sprint 5 (1-2 weeks)
- **Priority 2:** Continue transform migration
- **Goal:** Build library of production-ready transforms

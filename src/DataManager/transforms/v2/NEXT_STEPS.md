# Transforms V2 - Next Steps and Recommendations

## Current Status Summary

**✅ Phase 1, 2 & 3a Complete** - Core infrastructure and JSON configuration fully operational:

- **ElementRegistry**: Compile-time typed transform registry with TypedParamExecutor system
- **TypedParamExecutor**: Eliminates per-element parameter casts and type dispatch overhead
- **TransformPipeline**: Multi-step pipeline with execution fusion
- **Container Automatic Lifting**: Element transforms automatically work on containers
- **Binary Transforms**: Multi-input support via std::tuple with BinaryElementTransform
- **Advanced Features**: Ragged outputs, multi-input transforms (binary), time-grouped transforms
- **reflect-cpp Integration**: Automatic parameter serialization with validation
- **JSON Pipeline Loading**: Complete schema with PipelineDescriptor and comprehensive validation
- **Comprehensive Testing**: 50+ unit tests, 12+ fuzz tests, 18+ corpus files
- **Working Examples**: MaskArea, SumReduction, LineMinPointDist (binary) transforms with full tests

**Performance Characteristics:**
- Parameter cast: Once per pipeline (not per element)
- Type dispatch: Once per pipeline (not per element)
- Per-element overhead: ~2 virtual calls + 2 any_casts (input/output only)
- Fusion: Multiple transforms execute in single pass (no intermediate containers)
- Binary transforms: std::tuple-based with zero-copy view compatibility

## Recommended Next Steps

### Priority 1: Expand Test Coverage (High Impact, Low Complexity)

**Why:** Ensure robustness before migration; catch edge cases early

**Tasks:**
1. Add tests for multi-step pipelines (2-3 transforms chained)
2. Add tests for ragged output transforms
3. Add tests for multi-input transforms
4. Add tests for time-grouped transforms
5. Add performance benchmarks comparing V1 vs V2
6. Add tests for error handling (invalid transform names, type mismatches)

**Estimated Effort:** 1-2 days

**Files to Create:**
- `tests/DataManager/TransformsV2/test_pipeline.test.cpp`
- `tests/DataManager/TransformsV2/test_ragged_output.test.cpp`
- `tests/DataManager/TransformsV2/test_multi_input.test.cpp`
- `tests/DataManager/TransformsV2/test_time_grouped.test.cpp`
- `tests/DataManager/TransformsV2/benchmark_transforms.test.cpp`

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

**Benefits:**
- Validate container lifting works for all data types
- Identify missing container trait mappings
- Test pipeline composition with real transforms
- Compare performance V1 vs V2

### Priority 3: Runtime Configuration via JSON (High Impact, High Complexity)

**Why:** Enable UI pipeline builder; allow users to save/load analysis workflows

**Status:** ✅ Phase 3a Complete, Phase 3b In Progress

#### 3a. Parameter Serialization with reflect-cpp ✅ COMPLETE
- ✅ Defined parameter structs using reflect-cpp with validators
- ✅ Added `rfl::json::write()` and `rfl::json::read()` support
- ✅ Added parameter validation (min/max, type constraints)
- ✅ Updated all parameter structs (MaskAreaParams, SumReductionParams, LineMinPointDistParams)
- ✅ Comprehensive unit tests (20+ test cases)
- ✅ Comprehensive fuzz tests (5 fuzz functions with corpus)

**Files Created:**
- ✅ `src/DataManager/transforms/v2/examples/ParameterIO.hpp`
- ✅ `tests/DataManager/TransformsV2/test_parameter_io.test.cpp`
- ✅ `tests/fuzz/unit/DataManager/transforms/fuzz_v2_parameter_io.cpp`
- ✅ `tests/fuzz/corpus/v2_parameters/` (8 corpus files)

#### 3b. Pipeline JSON Schema and Loading ✅ COMPLETE ✅ COMPLETE
- ✅ Defined JSON schema for pipeline descriptors (PipelineDescriptor, PipelineStepDescriptor)
- ✅ Added pipeline serialization/deserialization with reflect-cpp
- ✅ Added comprehensive validation (transform existence, parameter types, metadata)
- ✅ Support for optional fields (description, enabled flag, tags)
- ✅ Comprehensive unit tests (25+ test cases)
- ✅ Comprehensive fuzz tests (7 fuzz functions with corpus)

**Files Created:**
- ✅ `src/DataManager/transforms/v2/examples/PipelineLoader.hpp`
- ✅ `tests/DataManager/TransformsV2/test_pipeline_loader.test.cpp`
- ✅ `tests/fuzz/unit/DataManager/transforms/fuzz_v2_pipeline_loader.cpp`
- ✅ `tests/fuzz/corpus/v2_pipelines/` (10 corpus files)

**JSON Schema Implemented:**
```json
{
  "metadata": {
    "name": "Whisker Analysis",
    "version": "1.0",
    "description": "Calculate whisker properties",
    "author": "Lab Name"
  },
  "steps": [
    {
      "step_id": "area_calc",
      "transform_name": "CalculateMaskArea",
      "parameters": {
        "scale_factor": 2.5,
        "min_area": 10.0
      },
      "description": "Calculate mask area",
      "enabled": true,
      "tags": ["geometry", "measurement"]
    }
  ]
}
```

#### 3c. Type-Erased Pipeline Factory (Next Step)

#### 3c. Type-Erased Executor Interface
- Define `IPipelineExecutor` interface for UI integration
- Implement type erasure for compiled pipelines
- Add progress reporting callbacks
- Add cancellation support

**Files to Create:**
- `src/DataManager/transforms/v2/runtime/PipelineExecutor.hpp`
- `tests/DataManager/TransformsV2/test_pipeline_executor.test.cpp`

**Estimated Effort:** 2-3 days

**Total for Priority 3:** 9-13 days

### Priority 4: UI Integration (High Impact, High Complexity)

**Why:** Make V2 transforms accessible to users; enable visual pipeline building

**Tasks:**
1. Add V2 transforms to DataTransform_Widget dropdown
2. Create pipeline builder UI (drag-drop transform nodes)
3. Auto-generate parameter forms from metadata
4. Add pipeline save/load functionality
5. Add pipeline visualization (DAG view)
6. Add execution progress reporting

**Estimated Effort:** 2-3 weeks

**Dependencies:** Priority 3 (runtime configuration) must be complete

### Priority 5: Advanced Features (Lower Priority, Various Complexity)

#### 5a. C++20 Ranges for True Lazy Evaluation (Medium-High Impact, High Complexity)

**Why:** Further reduce memory usage for long pipelines; enable on-demand computation

**Current State:** Pipeline fusion eliminates intermediate containers but still materializes output
**Goal:** Support lazy pipelines that compute on-demand (e.g., for interactive visualization)

**Challenges:**
- Container types need range view support (some already have it)
- Time-indexed data complicates lazy evaluation (random access needed)
- EntityID tracking becomes more complex

**Estimated Effort:** 2-3 weeks

**Recommendation:** Defer until Priority 1-3 complete; may not provide significant benefit given time-indexed nature of data

#### 5b. Provenance Tracking (Medium Impact, Medium Complexity)

**Why:** Track which Mask2D produced which Line2D; useful for debugging and visualization

**Implementation:**
- Store parent EntityIDs in LazyTransformStorage
- Add `getParentEntityIds()` method to all data types
- Add `getAncestorEntityIds()` for multi-level queries
- Integrate with EntityRegistry

**Estimated Effort:** 1-2 weeks

#### 5c. Parallel Execution (Medium Impact, High Complexity)

**Why:** Utilize multi-core CPUs for expensive transforms

**Implementation:**
- Mark expensive transforms in metadata (`is_expensive = true`)
- Use thread pool for parallel element processing
- Add chunking strategy for load balancing
- Handle thread-local state for transforms

**Estimated Effort:** 2-3 weeks

**Note:** Many transforms are already fast; profile before implementing

#### 5d. Lazy Storage Strategy (Low-Medium Impact, High Complexity)

**Why:** Avoid materializing intermediate results for derived data

**Implementation:**
- Abstract storage behind `DataStorage<T>` interface
- Implement `LazyTransformStorage` that stores pipeline + input reference
- Compute on-demand when `getAt(index)` called
- Add caching strategy for frequently accessed elements

**Estimated Effort:** 2-3 weeks

**Note:** Complexity of time-indexed data with variable frame counts makes this challenging

## Recommended Roadmap

### ✅ Sprint 1 Complete (Completed)
- ✅ **Priority 1:** Comprehensive testing (50+ unit tests, 12+ fuzz tests)
- ✅ **Priority 3a:** Parameter serialization with reflect-cpp
- ✅ **Priority 3b:** Pipeline JSON schema and loading
- ✅ **Binary Transform Example:** LineMinPointDist with full test coverage

### Sprint 2 (Current - 1-2 weeks)
- **Priority 2:** Migrate 3-5 real transforms from V1
- **Priority 3c:** Type-erased executor interface for UI integration
- **Goal:** Validate system with real-world transforms and enable UI access

### Sprint 3 (1-2 weeks)
- **Priority 2:** Continue transform migration
- **Priority 1:** Additional testing for migrated transforms
- **Goal:** Build library of production-ready transforms

### Sprint 4 (2-3 weeks)
- **Priority 4:** UI integration
- **Goal:** Make V2 transforms available to users

### Sprint 5+ (Future)
- **Priority 5:** Advanced features as needed
- **Goal:** Continuous improvement based on user feedback

## Key Decision Points

### Should we implement C++20 ranges?

**Pros:**
- Idiomatic modern C++
- Potential memory savings for long pipelines
- Composability with standard library algorithms

**Cons:**
- High complexity
- Time-indexed data complicates lazy evaluation
- Current fusion already provides most benefits
- Limited benefit for typical 2-3 step pipelines

**Recommendation:** Defer until Priority 1-4 complete. Re-evaluate based on:
- User feedback on memory usage
- Profiling showing long pipeline memory issues
- Demand for on-demand computation

### Should we use reflect-cpp for parameters?

**Pros:**
- Eliminates boilerplate JSON serialization
- Built-in validation support
- Cleaner parameter definitions

**Cons:**
- Additional dependency (already in project)
- Learning curve for developers
- Limited customization of serialization

**Recommendation:** Yes, use it. The project already depends on reflect-cpp, and the benefits outweigh the costs.

### Should we implement provenance tracking?

**Pros:**
- Useful for debugging ("which mask produced this line?")
- Enables data lineage visualization
- Helpful for complex multi-step analysis

**Cons:**
- Additional memory overhead (store parent IDs)
- Complexity in lazy evaluation scenarios
- May not be critical for typical workflows

**Recommendation:** Implement after Priority 3 complete. Add as opt-in feature (can be disabled for memory-constrained scenarios).

## Migration Strategy

### Parallel Operation Period
- Both V1 and V2 systems operational
- V2 transforms registered alongside V1
- UI shows both options
- Users can test and compare

### Gradual Migration
1. Migrate simple transforms first (area, centroid, etc.)
2. Gather user feedback
3. Migrate complex transforms
4. Monitor performance and usability

### Deprecation Plan
1. Mark V1 transforms as deprecated (UI badge/tooltip)
2. Provide migration guide for custom transforms
3. Set deprecation timeline (3-6 months)
4. Remove V1 after migration complete

## Success Metrics

### Technical Metrics
- ✅ All V1 transforms migrated to V2
- ✅ Performance equal or better than V1 (benchmark tests)
- ✅ Zero regressions in existing workflows
- ✅ Test coverage >80%

### User Experience Metrics
- ✅ Users can build pipelines via UI
- ✅ Users can save/load pipelines
- ✅ Parameter validation prevents errors
- ✅ Execution time visible to users
- ✅ Error messages are clear and actionable

### Developer Experience Metrics
- ✅ New transform <100 lines of code
- ✅ Parameter handling automatic (reflect-cpp)
- ✅ Registration is one line
- ✅ Unit testing is straightforward
- ✅ Documentation is comprehensive

## Questions to Resolve

1. **Memory vs Speed Tradeoff:** Should we optimize for minimal memory (lazy evaluation) or speed (eager fusion)?
   - **Current approach:** Fusion (speed priority)
   - **Recommendation:** Profile real workloads before changing

2. **Type Safety vs Flexibility:** How much runtime type checking should we allow?
   - **Current approach:** Compile-time types, runtime dispatch at boundaries
   - **Recommendation:** Keep current approach, very flexible already

3. **UI Complexity:** How sophisticated should the pipeline builder be?
   - **Options:** Simple dropdown list vs visual node graph editor
   - **Recommendation:** Start simple (list), add visual editor later if needed

4. **Backward Compatibility:** Should V2 support V1 pipeline JSON format?
   - **Recommendation:** No, too much complexity. Provide migration tool instead.

## Conclusion

The V2 transform system has a solid foundation with Phase 1 & 2 complete. The TypedParamExecutor system successfully eliminates per-element overhead while maintaining type safety.

**Recommended immediate focus:**
1. **Real-world validation** (Priority 2) - Migrate 3-5 actual transforms from V1
2. **Type-erased executor** (Priority 3c) - Enable UI integration with IPipelineExecutor interface
3. **Additional testing** (Priority 1) - Test migrated transforms and container-level binary execution

Advanced features (ranges, provenance, parallelization) should be deferred until the core system is battle-tested with real user workflows.

**Estimated timeline to production-ready:**
- ✅ Phase 3a Complete: Parameter serialization and JSON loading operational
- Remaining with Priorities 2-4: 6-9 weeks
- With Priorities 2-3c only: 3-5 weeks (can ship without full UI initially)

The system is well-architected with JSON configuration now operational. The next phase should focus on:
1. Migrating real transforms to validate the system
2. Creating type-erased executor interface for UI integration
3. Building the UI pipeline builder

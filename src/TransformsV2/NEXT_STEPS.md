# Transforms V2 - Next Steps

## Current Status

The V2 transformation system is **feature-complete** for core infrastructure:

- ✅ ElementRegistry with TypedParamExecutor
- ✅ TransformPipeline with execution fusion and view-based lazy evaluation
- ✅ Container automatic lifting and native container transforms
- ✅ Multi-input transforms (binary via tuple)
- ✅ reflect-cpp parameter serialization with validation
- ✅ JSON pipeline loading with pre-reductions and parameter bindings
- ✅ PipelineValueStore for computed values
- ✅ RangeReductions for trial-level analysis
- ✅ GatherResult integration with `buildTrialStore()` and value projections
- ✅ Provenance tracking (LineageRegistry, EntityResolver)
- ✅ DataManager integration (`load_data_from_json_config_v2`)
- ✅ Comprehensive testing (50+ unit tests, 12+ fuzz tests)

## Recommended Next Steps

### Priority 1: UI Integration (High Impact)

**Tasks:**
1. Add V2 transforms to DataTransform_Widget dropdown
2. Create pipeline builder UI (drag-drop transform nodes)
3. Auto-generate parameter forms from transform metadata
4. Add pipeline save/load functionality
5. Add execution progress reporting

**Estimated Effort:** 2-3 weeks

### Priority 2: Migrate Real Transforms from V1 (High Impact)

**Recommended Candidates:**
1. **Skeletonize** (Mask2D → Mask2D) - Image processing
2. **Centroid** (Mask2D → Point2D) - Geometry extraction
3. **MaskToLine** (Mask2D → Line2D) - Curve fitting
4. **LineAngle** (Line2D → float) - Geometric measurement
5. **Normalize** (AnalogTimeSeries → AnalogTimeSeries) - Statistics

**For each transform:**
1. Extract element-level function from V1 operation class
2. Define parameter struct
3. Register in transform registry
4. Add unit tests
5. (Optional) Keep V1 version for comparison

**Estimated Effort:** 3-5 days per transform

### Priority 3: Advanced Features (Lower Priority)

#### Parallel Execution

- Mark expensive transforms in metadata (`is_expensive = true`)
- Use thread pool for parallel element processing
- Add chunking strategy for load balancing

**Estimated Effort:** 2-3 weeks

**Note:** Profile first - many transforms are already fast.

## Performance Characteristics

Current system achieves:
- Parameter cast: Once per pipeline (not per element)
- Type dispatch: Once per pipeline (not per element)
- Fusion: Multiple transforms in single pass (no intermediate containers)
- View mode: Zero intermediate allocations

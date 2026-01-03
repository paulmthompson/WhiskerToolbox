# CorePlotting Code Review Roadmap

This document consolidates findings from two comprehensive code reviews and provides a prioritized action plan for improvements.

---

## Priority 1: Critical Issues

### 1.1 Floating Point Precision Risk in TimeSeriesMapper

**File**: [Mappers/TimeSeriesMapper.hpp](Mappers/TimeSeriesMapper.hpp#L88-L97)

**Issue**: Absolute time indices are cast directly to `float`:
```cpp
float x = static_cast<float>(time_frame.getTimeAtIndex(event_with_id.event_time));
```

**Problem**: 
- IEEE 754 32-bit float has only ~7 decimal digits of precision
- For microsecond timestamps since epoch or long recordings (>1 hour at 30kHz ≈ 10⁸ samples), this causes:
  - "Shaking polygon" jitter artifacts
  - Inability to distinguish adjacent samples when zoomed in

**Fix Options** (choose one):
1. **Double for World Coordinates**: Change `RenderableScene` matrices and `MappedElement` to use `double`/`glm::dvec2`
2. **Relative Encoding (Recommended)**: Subtract `view_state.time_start` from absolute time before casting to float. Render everything relative to current view window start.

**Action Items**:
- [ ] Update `MappedElement` to support camera-relative encoding
- [ ] Add `view_origin` parameter to `mapEvents`, `mapEventsInRange`, `mapIntervals`, `mapIntervalsInRange`, `mapAnalogSeries*` functions
- [ ] Subtract view origin before `static_cast<float>` in all mappers
- [ ] Update `SceneBuilder` to handle relative coordinates consistently

**Affected Functions**:
- `TimeSeriesMapper::mapEvents()`
- `TimeSeriesMapper::mapEventsInRange()`
- `TimeSeriesMapper::mapIntervals()`
- `TimeSeriesMapper::mapIntervalsInRange()`
- `TimeSeriesMapper::mapAnalogSeries()`
- `TimeSeriesMapper::mapAnalogSeriesWithIndices()`
- `TimeSeriesMapper::mapAnalogSeriesFull()`
- `RasterMapper::mapEventsRelative()` (already uses relative time)

---

### 1.2 Spatial Index Synchronization in SceneBuilder

**File**: [SceneGraph/SceneBuilder.hpp](SceneGraph/SceneBuilder.hpp#L333-L347)

**Issue**: Range-based methods (`addGlyphs`, `addRectangles`) populate `_pending_spatial_inserts`, but low-level batch methods (`addGlyphBatch`, `addPolyLineBatch`, `addRectangleBatch`) do not.

**Consequence**: Manual batches (e.g., from `GapDetector`) bypass the pending queue. When `build()` calls `buildSpatialIndexFromPending()`, these batches aren't indexed.

**Action Items**:
- [ ] Either: Update `addGlyphBatch` / `addRectangleBatch` to also populate `_pending_spatial_inserts`
- [ ] Or: Update `build()` to call `buildSpatialIndex()` which iterates all batches, not just pending inserts
- [ ] Document clearly which methods auto-populate spatial index

---

## Priority 2: Performance Optimizations

### 2.1 Memory Allocation in SceneBuilder

**File**: [SceneGraph/SceneBuilder.hpp](SceneGraph/SceneBuilder.hpp#L304-L318)

**Issue**: `_pending_spatial_inserts` allocates heap memory every frame, then immediately drains into QuadTree in `build()`.

**Optimization**: Since `setBounds()` must be called before adding glyphs (per comments), insert directly into QuadTree:

```cpp
template<MappedElementRange R>
SceneBuilder & addGlyphs(...) {
    if (!_scene.spatial_index) {
        throw std::runtime_error("Bounds must be set before adding glyphs");
    }
    for (auto const & elem : elements) {
        batch.positions.emplace_back(elem.x, elem.y);
        _scene.spatial_index->insert(elem.x, elem.y, elem.entity_id);  // Direct insertion
    }
}
```

**Action Items**:
- [ ] Remove `_pending_spatial_inserts` vector
- [ ] Initialize QuadTree in `setBounds()` 
- [ ] Insert directly into QuadTree in `addGlyphs` and `addRectangles`
- [ ] Remove `buildSpatialIndexFromPending()` helper

---

### 2.2 Allocation in GapDetector::transform

**File**: [Transformers/GapDetector.cpp](Transformers/GapDetector.cpp#L52-L65)

**Issue**: Explicit materialization into vectors negates range-based architecture benefits:
```cpp
std::vector<float> time_values;
time_values.reserve(time_indices.size());  // Allocation 1
std::vector<float> data(data_span.begin(), data_span.end());  // Allocation 2
```

**Action Items**:
- [ ] Rewrite `transform()` to use `segmentByGaps` template logic directly on `AnalogTimeSeries` view
- [ ] Remove intermediate `std::vector` allocations
- [ ] Consider keeping vector overload only for unit testing

---

### 2.3 SceneBuilder Vertex Resizing

**File**: [SceneGraph/SceneBuilder.hpp](SceneGraph/SceneBuilder.hpp#L401-L407)

**Issue**: `addPolyLine` pushes vertices one by one, potentially causing reallocations:
```cpp
for (auto const & v : vertices) {
    batch.vertices.push_back(v.x);
    batch.vertices.push_back(v.y);
}
```

**Action Items**:
- [ ] Check if range satisfies `std::ranges::sized_range`
- [ ] If so, call `batch.vertices.reserve(batch.vertices.size() + std::ranges::size(vertices) * 2)` before loop
- [ ] Apply same optimization to `addPolyLines`

---

### 2.4 RasterMapper::mapTrials Materialization

**File**: [Mappers/RasterMapper.hpp](Mappers/RasterMapper.hpp#L126)

**Issue**: `mapTrials` returns `std::vector<MappedElement>`, forcing evaluation despite `mapEventsRelative` already being a view.

**Action Items**:
- [ ] Use `std::ranges::join` or coroutine generator to make this lazy
- [ ] Maintain ownership semantics as needed

---

## Priority 3: Logic Errors & Edge Cases

### 3.1 RectangleInteractionController Constraint Logic

**File**: [Interaction/RectangleInteractionController.cpp](Interaction/RectangleInteractionController.cpp#L93-L97)

**Issue**: `constrain_to_x_axis` hardcodes `y=0.0f`:
```cpp
if (_config.constrain_to_x_axis) {
    _current_bounds = glm::vec4{x, 0.0f, width, _config.viewport_height};
}
```

**Problem**: If layout engine positioned series at `y=100.0f`, the preview appears at wrong vertical position.

**Action Items**:
- [ ] Pass series layout bounds to controller, OR
- [ ] Add `_config.y_min` / `_config.y_max` fields, OR
- [ ] Have PreviewRenderer ignore controller's Y and draw full-height
- [ ] Update `start()` method to accept series Y position

---

### 3.2 Hit Testing Interval Logic

**File**: [Interaction/SceneHitTester.hpp](Interaction/SceneHitTester.hpp#L140)

**Issue**: In `queryIntervals`:
```cpp
float dist = y_dist;  // For intervals, X is always inside
```

**Problem**: For overlapping intervals on the same row, X distance matters for determining which interval was clicked (especially at edges or with transparency).

**Action Items**:
- [ ] If `y_dist == 0` (inside Y), factor in distance to interval center in X
- [ ] Or strictly return 0 for "most centered" prioritization

---

### 3.3 RasterMapper Coordinate Mismatch Risk

**File**: [Mappers/RasterMapper.hpp](Mappers/RasterMapper.hpp#L57-L60)

**Issue**: `mapEventsRelative` outputs X as relative time, but `SceneHitTester` and `InverseTransform::worldXToTimeIndex` assume absolute time.

**Action Items**:
- [ ] Document that RasterMapper outputs require paired ViewState acknowledging relative X-axis
- [ ] Or distinguish "Time Space" from "World Space" in `HitTestResult`
- [ ] Add documentation/comments warning about coordinate system assumptions

---

## Priority 4: Code Cleanup

### 4.1 Delete Deprecated `findIntervalEdge`

**File**: [Interaction/SceneHitTester.hpp](Interaction/SceneHitTester.hpp#L165-L175)

**Issue**: Marked `@deprecated` in favor of `findIntervalEdgeByEntityId`. Delete it now.

**Action Items**:
- [ ] Remove `findIntervalEdge()` method declaration
- [ ] Remove implementation (if in .cpp)
- [ ] Update any remaining callers to use `findIntervalEdgeByEntityId()`

---

### 4.2 Review SpanLineView Usage

**File**: [Mappers/MappedLineView.hpp](Mappers/MappedLineView.hpp#L73-L110)

**Issue**: `SpanLineView`, `OwningLineView`, and generic `MappedLineView` all exist. Most mappers return `OwningLineView` or lazy ranges.

**Action Items**:
- [ ] Audit codebase for `SpanLineView` usage
- [ ] If unused by `SceneBuilder` or any consumer, remove to reduce API surface
- [ ] Document remaining line view types and when to use each

---

### 4.3 Review Generic createModelMatrix Usage

**File**: [CoordinateTransform/Matrices.cpp](CoordinateTransform/Matrices.cpp#L25-L30)

**Issue**: `createModelMatrix(float scale_x, float scale_y, float translate_x, float translate_y)` may be unused since `SeriesMatrices.hpp` provides `createModelMatrix(LayoutTransform)`.

**Action Items**:
- [ ] Audit codebase for generic `createModelMatrix` usage
- [ ] If unused, remove to reduce API surface
- [ ] If used, document when to prefer each overload

---

### 4.4 Update or Remove Interaction/ROADMAP.md

**File**: [Interaction/ROADMAP.md](Interaction/ROADMAP.md)

**Issue**: References "Phase 5" and "Phase 6" which may be outdated.

**Action Items**:
- [ ] Review if roadmap is current
- [ ] Update to reflect current state, OR
- [ ] Remove if no longer relevant

---

## Priority 5: Architecture Improvements

### 5.1 GapDetector Decoupling from Rendering Layer

**File**: [Transformers/GapDetector.hpp](Transformers/GapDetector.hpp)

**Issue**: Mappers use lazy ranges, but `GapDetector` forces materialization into `RenderablePolyLineBatch` immediately, breaking the pipeline:

- **Current**: Data → Mapper (Range) → GapDetector (Allocates Batch) → SceneBuilder
- **Ideal**: Data → Mapper (Range) → GapAdaptor (Range) → SceneBuilder

**Options**:
1. **Range Adaptor**: Rewrite as C++20 range adaptor (`gap_view`) that yields `std::subrange`s or modifies `line_start` signals lazily
2. **Middle Ground**: Return `std::vector<std::vector<MappedVertex>>` or flat vector with cut indices instead of `RenderablePolyLineBatch`

**Note**: Since OpenGL requires contiguous buffers for `glMultiDrawArrays`, some materialization is pragmatically necessary. The goal is to decouple from rendering concerns.

**Action Items**:
- [ ] Change `GapDetector::transform()` to return raw geometry (e.g., `SegmentedLineData` struct with vertices + segment indices)
- [ ] Let `SceneBuilder` convert to `RenderablePolyLineBatch`
- [ ] Or implement `gap_view` range adaptor for full lazy evaluation

---

### 5.2 Stronger Coordinate Typing

**Issue**: `float world_x` vs `float canvas_x` is easy to mix up despite `InverseTransform.hpp` helpers.

**Action Items**:
- [ ] Consider strong typedefs or simple structs:
  ```cpp
  struct WorldCoord { float x, y; };
  struct CanvasCoord { float x, y; };
  struct NDCCoord { float x, y; };
  ```
- [ ] Update function signatures to use typed coordinates
- [ ] Evaluate compile-time overhead vs. safety benefit

---

### 5.3 Clearer Naming in LayoutTransform/SeriesLayout

**Issue**: `SeriesLayout::y_transform` naming is ambiguous - is it Data→World or Normalized→World?

**Action Items**:
- [ ] Consider renaming helpers in `NormalizationHelpers`:
  - `createDataToNormalized()`
  - `createNormalizedToWorld()`
- [ ] Add more explicit documentation about transform composition chain

---

### 5.4 Add LayoutTransform Validation

**File**: [Layout/LayoutTransform.hpp](Layout/LayoutTransform.hpp)

**Issue**: `SeriesMatrices.cpp` has excellent validation (`validateOrthoParams`). Similar validation should exist for `LayoutTransform`.

**Action Items**:
- [ ] Add `validateLayoutTransform()` to ensure gain is never NaN or Infinity
- [ ] Call during `compose()` or `apply()` in debug builds
- [ ] Prevent bad values from propagating through layout engine

---

## Priority 6: Modernization & Polish

### 6.1 Add constexpr Where Possible

**Files**: 
- [Layout/NormalizationHelpers.hpp](Layout/NormalizationHelpers.hpp)
- [Layout/LayoutTransform.hpp](Layout/LayoutTransform.hpp)

**Action Items**:
- [ ] Mark `NormalizationHelpers` functions as `constexpr` where applicable
- [ ] Mark `LayoutTransform` methods as `constexpr` where applicable
- [ ] Verify no runtime-only operations prevent constexpr

---

### 6.2 Verify std::abs Include

**File**: [CoordinateTransform/InverseTransform.cpp](CoordinateTransform/InverseTransform.cpp)

**Action Items**:
- [ ] Verify `<cmath>` is included for `std::abs` on floats
- [ ] Ensure correct overload resolution (float vs int)

---

### 6.3 DESIGN.md vs Code Alignment

**File**: [DESIGN.md](DESIGN.md)

**Issue**: Design doc mentions "Compute shader for dense line data" but `RenderablePolyLineBatch` uses `std::vector`.

**Action Items**:
- [ ] If compute shaders are planned, ensure alignment (std430 layout, `glm::vec4` padding)
- [ ] Update DESIGN.md to reflect current implementation state
- [ ] Or add tracking issue for compute shader implementation

---

## Implementation Order

### Phase 1: Critical Fixes (Do First)
1. Floating point precision fix (1.1)
2. Spatial index synchronization (1.2)
3. RectangleInteractionController constraint logic (3.1)

### Phase 2: Performance
4. SceneBuilder memory allocation (2.1)
5. GapDetector allocation (2.2)
6. Vertex resizing optimization (2.3)

### Phase 3: Cleanup
7. Delete deprecated findIntervalEdge (4.1)
8. Audit and remove unused code (4.2, 4.3, 4.4)

### Phase 4: Architecture
9. GapDetector decoupling (5.1)
10. Stronger coordinate typing (5.2)
11. LayoutTransform validation (5.4)

### Phase 5: Polish
12. constexpr additions (6.1)
13. Documentation updates (6.2, 6.3)

---

## Summary Table

| Priority | Severity | File | Action |
|----------|----------|------|--------|
| 1.1 | **High** | TimeSeriesMapper.hpp | Fix precision: subtract view origin before float cast |
| 1.2 | **High** | SceneBuilder.hpp | Sync spatial index for manual batch additions |
| 2.1 | Medium | SceneBuilder.hpp | Remove `_pending_spatial_inserts`, direct QuadTree insert |
| 2.2 | Medium | GapDetector.cpp | Remove intermediate vector allocations |
| 2.3 | Low | SceneBuilder.hpp | Reserve vertex buffer capacity |
| 3.1 | Medium | RectangleInteractionController.cpp | Fix `constrain_to_x_axis` Y position |
| 3.2 | Low | SceneHitTester.hpp | Improve interval hit X-distance handling |
| 4.1 | Low | SceneHitTester.hpp | Delete deprecated `findIntervalEdge` |
| 4.2 | Low | MappedLineView.hpp | Audit/remove unused `SpanLineView` |
| 4.3 | Low | Matrices.cpp | Audit/remove unused `createModelMatrix` overload |
| 5.1 | Medium | GapDetector.hpp | Decouple from `RenderablePolyLineBatch` |

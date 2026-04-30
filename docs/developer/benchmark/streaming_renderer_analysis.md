# Streaming Polyline Renderer - Performance Analysis

## Executive Summary

Benchmarking reveals that for scrolling time series data, the **CPU-side vertex 
generation** is the primary bottleneck, not GPU buffer uploads. A ring buffer 
strategy that only generates new edge data can be **36,000x faster** than 
regenerating all vertices each frame.

## Benchmark Results

| Strategy | Time (100K points, scroll by 10) | Upload % |
|----------|----------------------------------|----------|
| Full Rebuild | 1.35ms | 100% |
| Dirty Region Detection | 396µs | 100% |
| Ring Buffer | **37ns** | **0.01%** |

## Current Architecture

```
OpenGLWidget::renderWithSceneRenderer()
    └── addAnalogBatchesToBuilder()
        └── buildAnalogSeriesBatchSimplified()
            └── TimeSeriesMapper::mapAnalogSeriesWithIndices()
                └── Iterates through ALL points in visible range
                    └── Creates new vertices vector
```

Every frame:
1. Iterates through all visible data points
2. Allocates new vertex vectors
3. Uploads entire buffer to GPU

## Optimization Approaches

### Approach 1: Renderer-Level Dirty Tracking (Implemented)
**Status:** Implemented in `StreamingPolyLineRenderer`
**Benefit:** Minimal (~3.5x)
**Reason:** The vertex VALUES change when scrolling, so dirty detection 
finds 100% dirty.

### Approach 2: Ring Buffer + Incremental Generation (Recommended)
**Status:** Design complete, implementation needed at batch builder level
**Benefit:** Up to 36,000x for small scroll steps

**Key Changes Required:**
1. `AnalogSeriesCache` - Maintain vertex data for extended time range
2. `buildAnalogSeriesBatchIncremental()` - Only generate new edge data
3. `StreamingPolyLineRenderer::updateEdgeData()` - Use glBufferSubData

```cpp
class AnalogSeriesCache {
    // Cached data covers [cached_start, cached_end]
    std::vector<float> vertices;
    TimeFrameIndex cached_start;
    TimeFrameIndex cached_end;
    
    // On scroll from [old_start, old_end] to [new_start, new_end]:
    // 1. If overlap exists, keep overlapping vertices
    // 2. Generate only [new_start, old_start) or (old_end, new_end]
};
```

### Approach 3: Persistent Mapped Buffers (GL 4.4+)
**Status:** Future enhancement
**Benefit:** Eliminates driver overhead for frequent updates

Uses `GL_MAP_PERSISTENT_BIT` + `GL_MAP_COHERENT_BIT` for zero-copy updates.

## Recommended Implementation Path

### Phase 1: Vertex Cache (High Impact)
Add caching to `TimeSeriesDataStore`:
- Cache vertex data for each analog series
- Track cached time range
- Provide `getVerticesForRange()` that returns cached or generates new

### Phase 2: Incremental Batch Building
Modify `buildAnalogSeriesBatch`:
- Accept cached vertices from Phase 1
- Only iterate over new time range
- Append/prepend to existing vertex buffer

### Phase 3: Ring Buffer GPU Upload
Modify `StreamingPolyLineRenderer`:
- Maintain ring buffer on GPU (3x visible capacity)
- Track write position
- Use glBufferSubData for edge updates only

## Files Modified

### New Files
- `src/PlottingOpenGL/Renderers/StreamingPolyLineRenderer.hpp/cpp` ✓
- `benchmark/PolyLineUpload.benchmark.cpp` ✓

### Future Changes (Phase 1-3)
- `src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.hpp` - Add vertex cache
- `src/WhiskerToolbox/DataViewer_Widget/SceneBuildingHelpers.cpp` - Incremental building
- `src/PlottingOpenGL/SceneRenderer.hpp` - Optional streaming mode

## Usage Example (Future)

```cpp
// In OpenGLWidget
void renderWithSceneRenderer() {
    // Check if we can use incremental update
    if (_streaming_enabled && canIncrementalUpdate(old_time_range, new_time_range)) {
        // Only generate new edge vertices
        auto edge_batch = buildIncrementalBatch(new_start, old_start);
        _streaming_renderer->appendEdgeData(edge_batch);
    } else {
        // Fall back to full rebuild
        auto full_batch = buildFullBatch();
        _streaming_renderer->uploadData(full_batch);
    }
}
```

## Conclusion

The `StreamingPolyLineRenderer` with dirty region detection provides modest 
improvement for the current architecture. For significant performance gains 
(1000x+), vertex caching must be implemented at the data store level.

The benchmark proves the concept works - implementing the full solution requires
refactoring the vertex generation pipeline, which is a larger undertaking but
would enable smooth 60fps scrolling even with millions of data points.

# CorePlotting Refactoring Roadmap

This document outlines the roadmap for consolidating the plotting architecture in WhiskerToolbox. The goal is to move from parallel, coupled implementations (DataViewer, Analysis Dashboard, SVGExporter) to a layered architecture centered around `CorePlotting`.

#### Remaining Work (Optional Future Cleanup)

The following are optional cleanups that can be done incrementally:

- [ ] **Migrate DisplayOptions to SeriesConfig**: Replace `NewAnalogTimeSeriesDisplayOptions` with `AnalogSeriesConfig`
- [ ] **Update SVGExporter**: Use same compose pattern for consistency

---

## Analysis Dashboard Updates
**Goal:** Apply same patterns to EventPlotWidget and other dashboard components.

### 5.1 EventPlotWidget (Raster Plot)
- [ ] Replace `vector<vector<float>>` data format with `DigitalEventSeries`
- [ ] Use `RowLayoutStrategy` from CorePlotting
- [ ] Add EntityId support for hover/click

### 5.3 SpatialOverlayWidget
- [ ] Already using ViewState ✓
- [ ] Add CorePlotting transformers for line/point data
- [ ] Unify tooltip infrastructure

### 5.4 Shared Infrastructure
- [ ] Create `PlotTooltipManager` that works with QuadTree results
- [ ] Standardize cursor change on hover (edge detection)

---

## Migration Checklist (Per Widget)

```
[ ] Widget uses CorePlotting LayoutEngine for positioning
[ ] Widget uses appropriate Mapper for its visualization type
[ ] Widget uses position-based SceneBuilder API
[ ] Widget uses CorePlotting ViewState (or TimeSeriesViewState)
[ ] Widget renders via RenderableScene (not inline vertex generation)
[ ] Widget uses appropriate hit testing strategy:
    [ ] QuadTree for sparse discrete data (events, points)
    [ ] Compute shader for dense line data (LineData, event-aligned traces)
    [ ] Time range query for intervals
[ ] SVG export works via same Mapper → SceneBuilder flow
[ ] Unit tests for Mapper output
[ ] Unit tests for layout calculations
[ ] Unit tests for coordinate transforms
```

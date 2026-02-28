

Line Display options


Tooltip infrastructure
Debounce timer

Temporal X Axis


Group Management

## Known Bug Patterns (check all plot widgets)

### 1. markDirty() in view-only setters causes scene rebuilds on zoom/pan

`EditorState::markDirty()` always emits `stateChanged()`. If view-only setters 
(`setXZoom`, `setYZoom`, `setPan`) call `markDirty()`, the connected 
`onStateChanged()` slot sets `_scene_dirty = true`, causing a full scene rebuild 
on every zoom/pan frame. The comments in these setters often say "No stateChanged() 
here" but `markDirty()` emits it anyway.

**Fix:** Remove `markDirty()` from view-only setters. The view state is still 
written to `_data.view_state` for serialization; it will be saved when the next 
real state change calls `markDirty()`.

**Fixed in:** EventPlotState (`setXZoom`, `setYZoom`, `setPan`)  
**Check:** LinePlotState and any other plot state classes with the same pattern.

### 2. rangeUpdated handler emits stateChanged() causing rebuild on resize/pan

`RelativeTimeAxisState::setRangeSilent()` emits `rangeUpdated`. If the state 
class's `rangeUpdated` handler calls `markDirty()` → `stateChanged()`, this 
creates a cascade: resize → `paintGL` → `viewBoundsChanged` → `syncTimeAxisRange()` 
→ `setRangeSilent()` → `rangeUpdated` → `markDirty()` → `stateChanged()` → 
`_scene_dirty = true` → full scene rebuild.

**Fix:** The `rangeUpdated` handler should only sync serialization data 
(`_data.time_axis = ...`) without calling `markDirty()` or emitting `stateChanged()`.

**Fixed in:** EventPlotState  
**Check:** LinePlotState and any other state class connecting to `rangeUpdated`.

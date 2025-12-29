# OpenGLWidget Legacy Cleanup TODO

Following the Phase 1-6 refactoring, there are several legacy components that can be safely removed from OpenGLWidget.

## Items to Remove

### 1. `commitInteraction()` method declaration ✅
- **Location:** `OpenGLWidget.hpp` line 496
- **Reason:** Method is declared but never implemented or called. The comment at `OpenGLWidget.cpp` line 921 states: "Note: commitInteraction() is no longer needed - handled by InteractionManager"
- [x] Remove declaration from header

### 2. `_gl_state.program` member ✅
- **Location:** `OpenGLResourceState` struct in `OpenGLWidget.hpp`
- **Reason:** The `QOpenGLShaderProgram * program` is never created - only null-checked and deleted in `cleanup()`. All shader management is now handled by `ShaderManager::instance()` and the individual renderers.
- [x] Remove `program` member from struct
- [x] Remove null-check and delete in `cleanup()`

### 3. `_gl_state.model` matrix ✅
- **Location:** `OpenGLResourceState` struct in `OpenGLWidget.hpp`
- **Reason:** The `model` matrix is declared but never used. Only `proj` and `view` matrices are used (in `drawAxis()` and `drawGridLines()`).
- [x] Remove `model` member from struct

### 4. `_gl_state.vbo` and `_gl_state.vao` members ✅
- **Location:** `OpenGLResourceState` struct in `OpenGLWidget.hpp`
- **Reason:** These VBO/VAO are created in `initializeGL()` and configured in `setupVertexAttribs()`, but never actually used for rendering. All rendering is now done via `SceneRenderer` and `AxisRenderer` which manage their own OpenGL resources.
- [x] Remove `vbo` and `vao` members from struct
- [x] Remove creation/destroy calls in `initializeGL()` and `cleanup()`
- [x] Remove unused includes (`QOpenGLBuffer`, `QOpenGLVertexArrayObject`, `QOpenGLShaderProgram`)

### 5. `setupVertexAttribs()` method ✅
- **Location:** `OpenGLWidget.hpp` line 483, `OpenGLWidget.cpp` lines 513-527
- **Reason:** This method sets up vertex attributes for the unused VBO/VAO. Should be removed along with the VBO/VAO.
- [x] Remove declaration from header
- [x] Remove implementation from cpp

### 6. `startIntervalCreationUnified()` and `startIntervalEdgeDragUnified()` private methods ✅
- **Location:** `OpenGLWidget.hpp` lines 508, 519; `OpenGLWidget.cpp` lines 923-986
- **Reason:** Comments say "for backwards compatibility" but these methods are never called. The constructor's signal handlers directly call `_interaction_manager->startIntervalCreation()` and `_interaction_manager->startEdgeDrag()`.
- [x] Remove declarations from header
- [x] Remove implementations from cpp

### 7. Legacy type aliases ✅
- **Location:** `OpenGLWidget.hpp` lines 107-109
- **Reason:** These aliases are not used anywhere in the codebase:
  ```cpp
  using AnalogSeriesData = DataViewer::AnalogSeriesEntry;
  using DigitalEventSeriesData = DataViewer::DigitalEventSeriesEntry;
  using DigitalIntervalSeriesData = DataViewer::DigitalIntervalSeriesEntry;
  ```
- [x] Remove all three type aliases

### 8. `TimeSeriesDefaultValues` namespace ✅
- **Location:** `OpenGLWidget.hpp` lines 672-687
- **Reason:** Marked as deprecated, delegates to `DataViewer::DefaultColors`. No usages of `TimeSeriesDefaultValues::` found in the codebase.
- [x] Remove entire namespace

### 9. Duplicated `InteractionMode` enum ✅
- **Location:** Global scope enum in `OpenGLWidget.hpp` (line 172) duplicates `DataViewer::InteractionMode` in `DataViewerInteractionManager.hpp` (line 37)
- **Reason:** Code already casts between them. Should use a single definition.
- [x] Remove global `InteractionMode` enum from `OpenGLWidget.hpp`
- [x] Add `using DataViewer::InteractionMode;` or use qualified name
- [x] Update signal `interactionModeChanged(InteractionMode)` if needed
- [x] Remove unnecessary `static_cast` conversions in cpp

---

## Estimated Impact

- **Lines removed:** ~100 lines
- **Member variables reduced:** 4 (program, model, vbo, vao)
- **Methods removed:** 3 (setupVertexAttribs, startIntervalCreationUnified, startIntervalEdgeDragUnified, commitInteraction declaration)
- **Risk:** Low - all items are confirmed unused via grep search

## Testing Strategy

1. Build project after each removal to catch any missed dependencies
2. Run unit tests: `ctest --preset linux-clang-release --output-on-failure`
3. Manual testing of DataViewer widget functionality:
   - Add/remove series
   - Pan and zoom
   - Create intervals via double-click
   - Modify intervals via edge dragging
   - Selection and tooltips

---

*Created: December 28, 2025*

# Python Integration Roadmap

> **Goal:** Allow users to pass WhiskerToolbox C++ data objects to Python, run analysis
> scripts (interactive console or `.py` files), and return new or modified data objects
> back to the DataManager — all with zero-copy where possible.

---

## Progress Summary

| Phase | Status | Completed |
|-------|--------|-----------|
| Phase 1 — Embedded Interpreter | **Complete** | Feb 2026 |
| Phase 2 — pybind11 Bindings | **Complete** | Feb 2026 |
| Phase 3 — DataManager Bridge | **Complete** | Feb 2026 |
| Phase 4 — Python_Widget UI | Not started | — |
| Phase 5 — Script Management | Not started | — |
| Phase 6 — Virtual Environment | Not started | — |
| Phase 7 — Advanced Features | Not started | — |

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Phase 1 — Embedded Interpreter & Environment Management](#2-phase-1--embedded-interpreter--environment-management)
3. [Phase 2 — Complete pybind11 Bindings (`whiskertoolbox_python`)](#3-phase-2--complete-pybind11-bindings-whiskertoolbox_python)
4. [Phase 3 — Python ↔ DataManager Bridge](#4-phase-3--python--datamanager-bridge)
5. [Phase 4 — Python_Widget UI](#5-phase-4--python_widget-ui)
6. [Phase 5 — Script Management & File Editor](#6-phase-5--script-management--file-editor)
7. [Phase 6 — Virtual Environment Support](#7-phase-6--virtual-environment-support)
8. [Phase 7 — Advanced Features](#8-phase-7--advanced-features)
9. [Zero-Copy Strategy](#9-zero-copy-strategy)
10. [Threading & Safety](#10-threading--safety)
11. [Testing Strategy](#11-testing-strategy)
12. [Open Questions & Risks](#12-open-questions--risks)

---

## 1. Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                     Python_Widget (Qt UI)                 │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │  Console Tab  │  │  Editor Tab  │  │ Properties     │  │
│  │  (REPL)       │  │  (.py files) │  │ (env, font,    │  │
│  │              │  │              │  │  venv selector) │  │
│  └──────┬───────┘  └──────┬───────┘  └────────┬───────┘  │
│         │                 │                    │          │
│         └─────────┬───────┘                    │          │
│                   ▼                            │          │
│         ┌─────────────────┐                    │          │
│         │ PythonEngine     │◄───────────────────┘          │
│         │  (interpreter    │                               │
│         │   management)    │                               │
│         └────────┬────────┘                               │
└──────────────────┼───────────────────────────────────────┘
                   │
                   ▼
         ┌─────────────────┐
         │ PythonBridge     │  ← Non-Qt layer
         │  (DataManager ↔  │
         │   Python scope)  │
         └────────┬────────┘
                  │
                  ▼
    ┌──────────────────────────┐
    │  whiskertoolbox_python    │  ← pybind11 module
    │  (C++ type bindings)      │
    │                           │
    │  TimeFrameIndex           │
    │  AnalogTimeSeries ──┐     │
    │  DigitalEventSeries │     │
    │  DigitalIntervalSeries    │
    │  LineData           │     │
    │  MaskData           │ zero│
    │  PointData          │ copy│
    │  TensorData ────────┘     │
    │  MediaData (read-only)    │
    │  DataManager (proxy)      │
    └──────────────────────────┘
```

### Library Dependency Graph

```
whiskertoolbox_python (pybind11 module)
  ├── DataManager (all data types)
  ├── TimeFrame
  ├── CoreGeometry
  ├── Entity
  └── Observer

PythonBridge (static lib, non-Qt)
  ├── whiskertoolbox_python (embeds the module)
  ├── DataManager
  └── pybind11::embed

Python_Widget (static lib, Qt)
  ├── PythonBridge
  ├── EditorState
  └── Qt6::Widgets
```

---

## 2. Phase 1 — Embedded Interpreter & Environment Management ✅

> **Status: COMPLETE** — All deliverables implemented and tested (14 tests, 59 assertions).

**Goal:** Boot a `pybind11::embed` interpreter inside the application, execute arbitrary
Python strings, and capture stdout/stderr.

### 2.1 Tasks

| # | Task | Details |
|---|------|---------|
| 1.1 | Add `pybind11::embed` to the build | Link `pybind11::embed` into a new `PythonBridge` static library. This library is **non-Qt** so the pybind11 bindings and the interpreter are decoupled from the UI. |
| 1.2 | Create `PythonEngine` class | Singleton-style class that owns the `py::scoped_interpreter` and the GIL. Provides `execute(string code) → PythonResult{stdout, stderr, success}`. |
| 1.3 | Redirect stdout/stderr | Use a custom Python `sys.stdout` / `sys.stderr` written in C++ (pybind11 class) that buffers output and emits it back to the engine. |
| 1.4 | Scope / namespace management | `PythonEngine` maintains a persistent `py::dict` for `globals` and `locals` so that variables persist across `execute()` calls (REPL behavior). Provide `resetNamespace()` to clear. |
| 1.5 | Error handling | Catch `py::error_already_set`, format tracebacks nicely, and return them in `PythonResult.stderr`. |

### 2.2 `PythonEngine` API Sketch

```cpp
// src/python_bindings/PythonEngine.hpp
struct PythonResult {
    std::string stdout_text;
    std::string stderr_text;
    bool success;
};

class PythonEngine {
public:
    PythonEngine();
    ~PythonEngine();

    // Execute a code string in the persistent namespace
    PythonResult execute(std::string const & code);

    // Execute a .py file
    PythonResult executeFile(std::filesystem::path const & path);

    // Reset the interpreter namespace (clear all user variables)
    void resetNamespace();

    // Inject a C++ object into the Python namespace
    void inject(std::string const & name, py::object obj);

    // Check if interpreter is initialized
    bool isInitialized() const;

    // Get the persistent globals dict (for bridge use)
    py::dict & globals();

private:
    std::unique_ptr<py::scoped_interpreter> _interpreter;
    py::dict _globals;
    py::dict _locals;
};
```

### 2.3 Build Changes

```cmake
# src/python_bindings/CMakeLists.txt  (revised)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# --- 1. The pybind11 extension module (importable from Python) ---
pybind11_add_module(whiskertoolbox_python bindings.cpp)
target_link_libraries(whiskertoolbox_python PRIVATE
    WhiskerToolbox::DataManager   # all data types
    WhiskerToolbox::TimeFrame
    WhiskerToolbox::CoreGeometry
    WhiskerToolbox::Entity
)

# --- 2. The embedded-interpreter bridge (linked into the app) ---
add_library(PythonBridge STATIC
    PythonEngine.hpp PythonEngine.cpp
    PythonBridge.hpp PythonBridge.cpp
)
target_link_libraries(PythonBridge
    PUBLIC  pybind11::embed Python3::Python
    PRIVATE WhiskerToolbox::DataManager
)
target_include_directories(PythonBridge PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
```

### 2.4 Deliverables

- [x] `PythonEngine` can boot interpreter, run code, return stdout/stderr
- [x] Unit test: `execute("print(2+2)")` → stdout = `"4\n"`, success = true
- [x] Unit test: `execute("raise ValueError('x')")` → stderr contains traceback, success = false
- [x] Namespace persists: `execute("x=5")` then `execute("print(x)")` → `"5\n"`

### 2.5 Implementation Notes

**Files delivered:**

| File | Purpose |
|------|---------|
| `src/python_bindings/PythonResult.hpp` | `PythonResult` struct (stdout, stderr, success) |
| `src/python_bindings/OutputRedirector.hpp` | C++ `sys.stdout`/`sys.stderr` replacement with `write()`/`flush()`/`drain()` |
| `src/python_bindings/PythonEngine.hpp` | `PythonEngine` class declaration |
| `src/python_bindings/PythonEngine.cpp` | Full implementation — interpreter lifecycle, `execute()`, `executeFile()`, `resetNamespace()`, `inject()`, traceback formatting |
| `src/python_bindings/PythonEngine.test.cpp` | 14 Catch2 test cases (59 assertions) |
| `src/python_bindings/CMakeLists.txt` | Added `PythonBridge` static library (`WhiskerToolbox::PythonBridge`) |
| `tests/python_bindings/CMakeLists.txt` | Test target `test_python_engine` |

**Key design decisions made during implementation:**

1. **Single static engine in tests** — Python can only be initialized/finalized once per process. Tests share a single `PythonEngine` instance and call `resetNamespace()` for isolation.
2. **Member init order** — `_interpreter` must be constructed in the member initializer list (before `_globals`) because `py::dict` default constructor calls `PyDict_New()`, which requires a live interpreter.
3. **`PYBIND11_EMBEDDED_MODULE(_wt_internal, m)`** — Registers `OutputRedirector` and a disabled `input()` function into an internal module imported at engine startup.
4. **Destructor safety** — Restores `sys.stdout`/`sys.stderr` to originals before interpreter teardown; relies on reverse member destruction order.

**Test coverage:**
- Interpreter init & version query
- stdout capture (simple, multi-line, `end=` parameter)
- stderr on SyntaxError, ValueError, NameError (with traceback)
- Namespace persistence (variables, functions, imports)
- `resetNamespace()` isolation
- `inject()` C++ → Python objects
- `getUserVariableNames()` filtering
- `input()` disabled (RuntimeError)
- Empty/whitespace/comment-only code
- `executeFile()` (success + missing file)
- Error recovery (engine usable after exceptions)
- stdout isolation between calls

---

## 3. Phase 2 — Complete pybind11 Bindings (`whiskertoolbox_python`)

**Goal:** Expose all core data types with zero-copy NumPy interop where possible.

### 3.1 Binding Plan by Data Type

#### 3.1.1 `TimeFrameIndex` (already bound — extend)
- Add arithmetic operators (`+`, `-`, `+=`, `-=`)
- Add `__hash__` for use in Python dicts/sets
- Add `__int__` conversion

#### 3.1.2 `TimeFrame`
- `getTotalFrameCount() → int`
- `getTimeAtIndex(TimeFrameIndex) → int`
- `getIndexAtTime(float, bool preceding=True) → TimeFrameIndex`

#### 3.1.3 `AnalogTimeSeries` ★ (zero-copy priority)
- **Zero-copy read:** Expose `getAnalogTimeSeries()` as a NumPy array via the buffer protocol. The `std::span<float const>` maps directly to a 1-D `numpy.ndarray` with `dtype=float32`. The array should be **read-only** (non-writeable flag set) to prevent accidental mutation.
- **Zero-copy sub-range:** `getDataInTimeFrameIndexRange(start, end)` → read-only NumPy array
- **Write path:** Accept a NumPy array + time indices to construct a new `AnalogTimeSeries`
- `getNumSamples()`, `getAtTime()`, `getTimeSeries()`, `isView()`, `isLazy()`
- Factory: `AnalogTimeSeries(numpy_array, time_indices)` — copies data in

#### 3.1.4 `DigitalEventSeries` (already partially bound — extend)
- Add `view()` iteration support via `__iter__` yielding `(time_index, entity_id)` tuples
- Add `getEventsInRange(start, stop)` → list of `TimeFrameIndex`
- Add construction from Python list of ints

#### 3.1.5 `DigitalIntervalSeries`
- Bind `Interval` type: `start`, `end`, `__repr__`
- `addEvent(Interval)`, `addEvent(start, end)`
- `removeInterval(Interval)`, `size()`, `clear()`
- `__iter__` yielding `(Interval, entity_id)` tuples
- `hasIntervalAtTime()`, construction from list of `(start, end)` tuples

#### 3.1.6 `LineData`
- Bind `Line2D` → list of `Point2D` or NumPy `(N, 2)` float array
- `addAtTime(time, line)`, `getDataByEntityId(id) → Line2D`
- `__iter__` for iterating over `(time, entity_id, line)` entries
- Zero-copy: expose `Line2D` points as read-only `(N, 2)` NumPy view when contiguous

#### 3.1.7 `MaskData`
- Bind `Mask2D` → NumPy `(N, 2)` uint32 array (point list) or `(H, W)` bool array (rasterized)
- `addAtTime(time, mask)`, `getDataByEntityId(id)`
- Provide Python helper to convert between point-list and raster representations

#### 3.1.8 `PointData`
- Bind `Point2D<float>` with `.x`, `.y`, `__repr__`
- `addAtTime(time, point_or_points)`
- `getDataByEntityId(id) → Point2D`
- `__iter__` yielding `(time, entity_id, point)` tuples

#### 3.1.9 `TensorData` ★ (zero-copy priority)
- **Zero-copy read:** `flatData()` → read-only NumPy array reshaped to `shape()`
- **Metadata:** `ndim()`, `shape()`, `columnNames()`, `rowType()`
- **Column access:** `getColumn(name_or_index)` → NumPy array (copy, since columns are non-contiguous)
- **Write path:** `TensorData.createND(numpy_array, shape, ...)` — create from NumPy
- `hasNamedColumns()`, `isEmpty()`, `isContiguous()`

#### 3.1.10 `MediaData` (read-only)
- `getRawData8(frame) → numpy (H, W) uint8 array` (copy — data is per-frame)
- `getRawData32(frame) → numpy (H, W) float32 array`
- `getHeight()`, `getWidth()`, `getTotalFrameCount()`, `getMediaType()`
- No write path initially (media is loaded from files)

#### 3.1.11 `DataManager` (proxy)
- `getData(key, type_hint) → typed object`
- `setData(key, object, time_key)` — register new data
- `deleteData(key) → bool`
- `getAllKeys() → list[str]`
- `getKeys(type_name) → list[str]`
- `getType(key) → str`
- `getTime(key) → TimeFrame`
- `getTimeKey(key) → str`

#### 3.1.12 Entity / Groups (lightweight)
- `EntityId`: `id`, `__repr__`, `__hash__`, `__eq__`
- Read-only access to group names and members via `DataManager` proxy

### 3.2 Binding File Organization

```
src/python_bindings/
├── CMakeLists.txt
├── bindings.cpp                   # PYBIND11_MODULE + sub-module init calls
├── bind_timeframe.cpp             # TimeFrameIndex, TimeFrame
├── bind_analog.cpp                # AnalogTimeSeries (zero-copy)
├── bind_digital_event.cpp         # DigitalEventSeries
├── bind_digital_interval.cpp      # DigitalIntervalSeries
├── bind_line.cpp                  # LineData, Line2D
├── bind_mask.cpp                  # MaskData, Mask2D
├── bind_point.cpp                 # PointData, Point2D
├── bind_tensor.cpp                # TensorData (zero-copy)
├── bind_media.cpp                 # MediaData (read-only)
├── bind_datamanager.cpp           # DataManager proxy
├── bind_entity.cpp                # EntityId, groups
├── numpy_utils.hpp                # Helpers for span↔numpy conversion
├── PythonEngine.hpp / .cpp        # Embedded interpreter
├── PythonBridge.hpp / .cpp        # DataManager ↔ Python scope bridge
└── PYTHON_INTEGRATION_ROADMAP.md  # This file
```

### 3.3 Deliverables

- [x] All data types bound with docstrings
- [x] NumPy zero-copy implemented for `AnalogTimeSeries` and `TensorData` (via `numpy_utils.hpp`)
- [x] Unit tests for each bound type (round-trip: C++ → Python → C++)
- [x] `import whiskertoolbox_python` works from an embedded interpreter

### 3.4 Implementation Notes

**Architecture change:** Switched from `pybind11_add_module` (standalone `.so`) to `PYBIND11_EMBEDDED_MODULE` compiled into the `PythonBridge` static library. A force-linkage function (`ensure_whiskertoolbox_bindings_linked()`) called from `PythonEngine` constructor prevents the linker from stripping `bindings.o` out of the static archive.

**Files delivered:**

| File | Purpose |
|------|---------|  
| `bind_module.hpp` | Forward declarations for all `init_*()` functions + force-linkage function |
| `numpy_utils.hpp` | Zero-copy `span_to_numpy_readonly()` and `numpy_to_vector()` helpers |
| `bindings.cpp` | `PYBIND11_EMBEDDED_MODULE(whiskertoolbox_python, m)` + `ensure_whiskertoolbox_bindings_linked()` |
| `bind_geometry.cpp` | `Point2D<float>`, `Point2D<uint32_t>`, `ImageSize` |
| `bind_timeframe.cpp` | `TimeFrameIndex` (extended: arithmetic, hash, int), `TimeFrame`, `Interval` |
| `bind_entity.cpp` | `EntityId`, `GroupDescriptor`, `EntityGroupManager` |
| `bind_analog.cpp` | `AnalogTimeSeries` with zero-copy `values` property |
| `bind_digital_event.cpp` | `DigitalEventSeries` (add/remove/clear/toList/toListWithIds) |
| `bind_digital_interval.cpp` | `DigitalIntervalSeries` (add/remove/toList/toListWithIds) |
| `bind_line.cpp` | `Line2D` (value type), `LineData` (ragged time series) |
| `bind_mask.cpp` | `Mask2D` (value type), `MaskData` (ragged time series) |
| `bind_point.cpp` | `PointData` (ragged time series) |
| `bind_tensor.cpp` | `TensorData` with zero-copy `values`/`flatValues` properties |
| `bind_media.cpp` | `MediaData` (read-only: frame access, metadata), `MediaType` enum |
| `bind_datamanager.cpp` | `DataManager` proxy with type-dispatched `getData()`, `setData()` overloads, `DataType` enum |
| `bindings.test.cpp` | 24 Catch2 test cases covering all bound types |

**Key design decisions:**

1. **PYBIND11_EMBEDDED_MODULE** — All bindings are compiled into the app, not as a standalone `.so`. This avoids shared library deployment issues.
2. **Force-linkage pattern** — `ensure_whiskertoolbox_bindings_linked()` is an empty function in `bindings.cpp` called from `PythonEngine()` to prevent static archive stripping.
3. **Zero-copy via `span_to_numpy_readonly()`** — Uses `py::detail::array_proxy()->flags &= ~NPY_ARRAY_WRITEABLE_` to mark arrays read-only.
4. **DataManager.getData() auto-dispatch** — Uses `getDataVariant()` + `std::visit` + `py::cast(ptr)` with `catch(py::cast_error)` fallback for unbound types (e.g., `RaggedAnalogTimeSeries` returns `None`).
5. **Type-specific setData overloads** — 7 overloads (one per data type) with pybind11 overload resolution, each constructing `TimeKey` from a string argument.
6. **Copy-based fallbacks** — Every type has `toList()` methods that work without NumPy at runtime.

**Test coverage (24 tests):**
- Module import
- Geometry types (Point2D, Point2DU32, ImageSize)
- TimeFrameIndex arithmetic, hash, int conversion
- TimeFrame construction and queries
- Interval construction and fields
- EntityId construction, hash, comparison
- AnalogTimeSeries (empty, from vectors, toList, getAtTime)
- DigitalEventSeries (add/remove/size/toList, construct from list)
- DigitalIntervalSeries (addInterval/addEvent/toList)
- Line2D construction and access (from points, from x/y vectors)
- LineData add and retrieve
- Mask2D construction (from points, from x/y vectors)
- MaskData add and retrieve
- PointData add and retrieve
- TensorData (createOrdinal2D, shape, toList, named columns)
- DataManager (setData/getData round-trip, type queries, key queries, delete, TimeFrame management)
- DataType enum

---

## 4. Phase 3 — Python ↔ DataManager Bridge

**Goal:** Let Python code access and modify the live DataManager, creating new data keys
and retrieving existing ones — while keeping the observer pattern working.

### 4.1 `PythonBridge` Class

```cpp
// src/python_bindings/PythonBridge.hpp
class PythonBridge {
public:
    explicit PythonBridge(std::shared_ptr<DataManager> dm, PythonEngine & engine);

    // Inject the DataManager proxy object into Python namespace as `dm`
    void exposeDataManager();

    // Convenience: inject a single data object by key
    void exposeData(std::string const & key, std::string const & python_name);

    // After script execution, scan Python namespace for new data objects
    // and register them in the DataManager
    std::vector<std::string> importNewData();

    // Expose a specific TimeFrame
    void exposeTimeFrame(std::string const & time_key, std::string const & python_name);

private:
    std::shared_ptr<DataManager> _dm;
    PythonEngine & _engine;
};
```

### 4.2 Workflow: User Perspective

```python
# Inside the WhiskerToolbox Python console:

# 1. The DataManager is always available as `dm`
keys = dm.getAllKeys()
print(keys)  # ['whisker_angle', 'lick_events', ...]

# 2. Get data (zero-copy where possible)
import numpy as np
angle = dm.getData("whisker_angle")   # returns AnalogTimeSeries wrapper
arr = np.array(angle, copy=False)     # zero-copy view!
print(arr.mean())

# 3. Do analysis
from scipy.signal import butter, filtfilt
b, a = butter(4, 0.1)
filtered = filtfilt(b, a, arr).astype(np.float32)

# 4. Create new data and push back
from whiskertoolbox_python import AnalogTimeSeries
ts = AnalogTimeSeries(filtered, angle.getTimeSeries())
dm.setData("whisker_angle_filtered", ts, dm.getTimeKey("whisker_angle"))
# → Now visible in WhiskerToolbox UI!
```

### 4.3 Observer Integration

When `dm.setData()` is called from Python, the C++ `DataManager::setData` fires
`notifyObservers()`. Since the Python call happens on the main thread (GIL held),
Qt widgets that observe the DataManager will update on next event-loop cycle.

**Important:** All Python execution must happen on the **main thread** or use
`QMetaObject::invokeMethod` to marshal DataManager mutations back to the main thread.
The observer pattern is not thread-safe.

### 4.4 Deliverables

- [x] `PythonBridge` injects `dm` proxy into Python namespace
- [x] `dm.getData("key")` returns typed Python wrapper
- [x] `dm.setData("key", obj, time_key)` registers data in C++ DataManager
- [x] Observer notifications fire correctly after Python mutations
- [x] Integration test: create data in Python → verify visible via C++ DataManager API

### 4.5 Implementation Notes

> **Status: COMPLETE** — All deliverables implemented and tested (27 tests, 90 assertions).

**Files delivered:**

| File | Purpose |
|------|--------|
| `src/python_bindings/PythonBridge.hpp` | `PythonBridge` class declaration — non-Qt bridge between DataManager and Python |
| `src/python_bindings/PythonBridge.cpp` | Full implementation — `exposeDataManager()`, `exposeData()`, `exposeTimeFrame()`, `importNewData()`, `execute()`, `executeFile()` |
| `src/python_bindings/PythonBridge.test.cpp` | 27 Catch2 test cases (90 assertions) |
| `tests/python_bindings/CMakeLists.txt` | Added `test_python_bridge` target |

**Key design decisions:**

1. **Module import before cast** — The embedded pybind11 module must be imported (`import whiskertoolbox_python`) before calling `py::cast()` on C++ types, since type registration happens lazily during module init.
2. **Shared pointer identity** — The `PythonBridge` injects the same `shared_ptr<DataManager>` that the C++ app owns, so mutations from Python are immediately visible in C++ and vice-versa.
3. **exposeData() uses Python-side dispatch** — Instead of `std::visit` on `DataTypeVariant` (which requires complete types for all variant alternatives), `exposeData()` delegates to the already-bound `DataManager.getData()` method in Python.
4. **importNewData() type probing** — Uses `py::cast<shared_ptr<T>>` with `catch(py::cast_error)` fallback to try each bound type. Non-data objects (ints, strings, modules) are silently skipped.
5. **Observer correctness** — Since all Python execution is synchronous on the main thread (Phase 1–5 threading model), `DataManager::setData()` calls from Python trigger `notifyObservers()` on the main thread — no marshalling needed.

**Test coverage (27 tests):**
- DataManager exposure (`exposeDataManager`, auto-expose via `execute()`)
- Shared pointer identity (C++ data visible from Python)
- getData for AnalogTimeSeries, DigitalEventSeries, missing keys
- setData from Python for all 6 writable types (Analog, DigitalEvent, DigitalInterval, Line, Point, Mask)
- Observer notifications on setData and deleteData
- importNewData: orphan discovery, skip-already-registered, multi-type, custom time key
- exposeData / exposeTimeFrame (inject individual objects, missing key handling)
- executeFile with dm available
- Error recovery (bridge usable after Python exceptions)
- resetNamespace + re-expose
- End-to-end workflows (C++ → Python filter → C++ verify, Python event/interval creation)

---

## 5. Phase 4 — Python_Widget UI

**Goal:** Build the interactive console and script editor in the existing Python_Widget
shell.

### 5.1 Console Tab (`PythonConsoleWidget`)

| Feature | Details |
|---------|---------|
| Input area | Multi-line `QPlainTextEdit` (bottom), monospace font, syntax highlighting |
| Output area | Read-only `QPlainTextEdit` (top), shows stdout/stderr in different colors |
| Execute | Shift+Enter or "Run" button sends input to `PythonEngine::execute()` |
| History | Up/Down arrow navigates command history (stored in state) |
| Auto-scroll | Toggle (already in state), scrolls output to bottom |
| Clear | Button to clear output pane |
| Interrupt | Button that calls `PyErr_SetInterrupt()` to stop long-running scripts |
| Tab-completion | Basic completion using Python's `rlcompleter` module |
| Prompt | `>>>` for new statements, `...` for continuation lines |

### 5.2 Script Editor Tab (`PythonEditorWidget`)

| Feature | Details |
|---------|---------|
| Editor | `QPlainTextEdit` with line numbers, monospace font, basic Python syntax highlighting |
| Open / Save / Save As | Standard file operations for `.py` files, path stored in state |
| Run Script | Executes the entire file via `PythonEngine::executeFile()` |
| Run Selection | Executes only highlighted text |
| Recent files | Stored in `PythonWidgetStateData` |
| Modified indicator | Asterisk in tab title when unsaved |
| Font size | Configurable via properties panel (already in state) |

### 5.3 Properties Panel (`PythonPropertiesWidget`)

| Feature | Details |
|---------|---------|
| Environment info | Show Python version, `sys.executable`, active venv |
| Venv selector | Dropdown to select / activate a virtual environment |
| Font size spinner | Connected to `PythonWidgetState::setFontSize()` |
| Auto-scroll toggle | Connected to `PythonWidgetState::setAutoScroll()` |
| Namespace inspector | Tree/table showing current Python variables, types, and shapes |
| Clear namespace | Button to call `PythonEngine::resetNamespace()` |
| Available data | List of DataManager keys with type, click to insert `dm.getData("key")` into console |

### 5.4 State Extensions

Add to `PythonWidgetStateData`:

```cpp
struct PythonWidgetStateData {
    std::string instance_id;
    std::string display_name = "Python Console";
    std::string last_script_path;     // existing
    bool auto_scroll = true;          // existing
    int font_size = 10;               // existing

    // New fields:
    std::vector<std::string> command_history;
    std::vector<std::string> recent_scripts;
    std::string venv_path;                       // active virtual environment
    std::string last_working_directory;
    bool show_line_numbers = true;
    std::string editor_content;                  // unsaved editor buffer
};
```

### 5.5 Python Syntax Highlighting

Create a `PythonSyntaxHighlighter : QSyntaxHighlighter` that highlights:
- Keywords (`def`, `class`, `import`, `if`, `for`, `while`, `return`, `with`, `as`, `try`, `except`, ...)
- Built-in functions (`print`, `range`, `len`, `type`, ...)
- Strings (single/double/triple-quoted)
- Comments (`#`)
- Numbers (int, float, hex, scientific)
- Decorators (`@`)
- `self`, `True`, `False`, `None`

### 5.6 Widget Layout

```
┌─────────────────────────────────────────────┐
│ Python_Widget (Center zone)                  │
│ ┌─────────────────────────────────────────┐  │
│ │ [Console] [Editor]           tabs       │  │
│ ├─────────────────────────────────────────┤  │
│ │                                         │  │
│ │  Output area (read-only)                │  │
│ │  >>> print("hello")                     │  │
│ │  hello                                  │  │
│ │                                         │  │
│ ├─────────────────────────────────────────┤  │
│ │  >>> _                  [Run] [Clear]   │  │
│ │  Input area                             │  │
│ └─────────────────────────────────────────┘  │
└─────────────────────────────────────────────┘

┌─────────────────────┐
│ Properties (Right)   │
│ ┌─────────────────┐  │
│ │ Environment      │  │
│ │ Python 3.12.x    │  │
│ │ venv: [dropdown] │  │
│ ├─────────────────┤  │
│ │ Settings         │  │
│ │ Font: [10] ▾    │  │
│ │ ☑ Auto-scroll   │  │
│ │ ☑ Line numbers  │  │
│ ├─────────────────┤  │
│ │ Namespace        │  │
│ │ x: int = 42     │  │
│ │ arr: ndarray     │  │
│ │   (1000,) f32   │  │
│ ├─────────────────┤  │
│ │ Data Keys        │  │
│ │ whisker_angle    │  │
│ │ lick_events      │  │
│ │ [Insert ▸]       │  │
│ └─────────────────┘  │
└─────────────────────┘
```

### 5.7 Deliverables

- [ ] Console tab: execute code, show output, command history
- [ ] Editor tab: open/save/run `.py` files
- [ ] Properties panel: env info, settings, namespace inspector
- [ ] Syntax highlighting for both console and editor
- [ ] State serialization of all new fields
- [ ] Keyboard shortcuts (Shift+Enter to run, Ctrl+L to clear, Up/Down for history)

---

## 6. Phase 5 — Script Management & File Editor

**Goal:** Support loading, editing, saving and running `.py` scripts with proper
working-directory management.

### 6.1 Tasks

| # | Task | Details |
|---|------|---------|
| 5.1 | Working directory | `PythonEngine` sets `os.chdir()` to the script's parent dir before execution. Also configurable in properties. |
| 5.2 | Script arguments | Allow passing `sys.argv` to scripts (text field in properties). |
| 5.3 | Auto-import prelude | On interpreter init, auto-execute: `import numpy as np; from whiskertoolbox_python import *`. Configurable. |
| 5.4 | Script templates | "New from template" menu: blank, AnalogTimeSeries filter, batch processing, etc. |
| 5.5 | Drag-and-drop | Drop `.py` file onto editor tab to open it. |
| 5.6 | Recent files menu | Stored in state, accessible from toolbar. |

---

## 7. Phase 6 — Virtual Environment Support

**Goal:** Detect, select, and activate Python virtual environments so users can
`import scipy`, `import sklearn`, etc.

### 7.1 Design

The embedded interpreter's `sys.path` must be modified to include the venv's
`site-packages`. Since `pybind11::embed` uses the compile-time Python, the venv
must match the same Python 3.12 version.

### 7.2 Tasks

| # | Task | Details |
|---|------|---------|
| 6.1 | Venv discovery | Scan common locations: `~/.virtualenvs/`, `~/.conda/envs/`, project-local `.venv/`, and user-configured paths. |
| 6.2 | Venv activation | Modify `sys.path` to prepend the venv's `site-packages`. Set `sys.prefix` and `VIRTUAL_ENV`. **Do not** re-exec the interpreter — just update paths. |
| 6.3 | Venv validation | Before activation, confirm the venv's Python version matches the embedded interpreter (3.12.x). Show warning if mismatched. |
| 6.4 | Package listing | After activation, `pip list` or `importlib.metadata` to show installed packages in properties panel. |
| 6.5 | Install packages | "Install package" dialog that runs `pip install <pkg>` via subprocess in the venv. |
| 6.6 | Persistence | Store selected venv path in `PythonWidgetStateData::venv_path`. Restore on next session. |
| 6.7 | Indicator | Show active venv name in console prompt and properties panel. |

### 7.3 Venv Activation Implementation

```cpp
void PythonEngine::activateVenv(std::filesystem::path const & venv_path) {
    // 1. Validate
    auto site_packages = findSitePackages(venv_path);  // e.g. lib/python3.12/site-packages

    // 2. Modify sys.path
    py::module_ sys = py::module_::import("sys");
    py::list path = sys.attr("path");
    path.attr("insert")(0, site_packages.string());

    // 3. Set sys.prefix
    sys.attr("prefix") = venv_path.string();
    sys.attr("exec_prefix") = venv_path.string();

    // 4. Set environment variable
    py::module_ os = py::module_::import("os");
    os.attr("environ")["VIRTUAL_ENV"] = venv_path.string();
}
```

### 7.4 Deliverables

- [ ] Venv dropdown in properties with autodiscovery
- [ ] Activation modifies `sys.path` — `import numpy` works when numpy is in the venv
- [ ] Version mismatch warning
- [ ] Persistence across sessions

---

## 8. Phase 7 — Advanced Features

These are lower-priority enhancements to build after the core is solid.

### 8.1 Async / Background Execution

- Run long scripts in a `QThread`, holding the GIL only during actual Python execution.
- Show a progress bar / cancel button.
- Use `PyErr_SetInterrupt()` + `signal.SIGINT` for cancellation.
- DataManager mutations from background threads must be marshalled to the main thread.

### 8.2 Variable Watch Window

- Show live-updating table of Python namespace variables.
- For NumPy arrays, show shape, dtype, min/max/mean.
- For WhiskerToolbox objects, show key, type, size.
- Refresh after each execution.

### 8.3 Plotting Integration

- `import matplotlib.pyplot as plt` → intercept `plt.show()` and render into a Qt widget
  using `matplotlib.backends.backend_qtagg.FigureCanvasQTAgg`.
- Or: provide a `wt.plot(analog_ts)` shortcut that opens a DataViewer_Widget.

### 8.4 Transform as Python Script

- Allow users to register a Python function as a `TransformOperation`.
- The function takes a `DataTypeVariant` proxy and returns a new one.
- Appears in the DataTransform_Widget alongside C++ transforms.

### 8.5 Batch Processing

- Run a script for each entity in a group or each interval in a `DigitalIntervalSeries`.
- Provide an iterator: `for entity in dm.getGroup("my_group"): ...`

### 8.6 Notebook-Style Interface

- Optional cell-based execution (like Jupyter) within the editor tab.
- Each cell remembers its output.
- Lower priority — the console + editor should cover most needs.

---

## 9. Zero-Copy Strategy

### 9.1 Read Path (C++ → Python)

| Data Type | Method | Zero-Copy? | NumPy dtype | Notes |
|-----------|--------|-----------|-------------|-------|
| `AnalogTimeSeries` | `getAnalogTimeSeries()` → `span<float const>` | **Yes** | `float32` | Set `ndarray.flags.writeable = False` |
| `AnalogTimeSeries` | `getDataInTimeFrameIndexRange()` → `span<float const>` | **Yes** | `float32` | Sub-span of same buffer |
| `TensorData` | `flatData()` → `span<float const>` | **Yes** | `float32` | Reshape to `shape()` in Python |
| `LineData` / `Line2D` | `Line2D::data()` → `vector<Point2D<float>>` | **Yes**\* | `float32 (N,2)` | If `Point2D` is `{float x,y}` with no padding, reinterpret as `(N,2)`. Verify with `static_assert(sizeof(Point2D<float>) == 2*sizeof(float))`. |
| `MaskData` / `Mask2D` | `Mask2D::data()` → `vector<Point2D<uint32_t>>` | **Yes**\* | `uint32 (N,2)` | Same reinterpret strategy |
| `PointData` | Single `Point2D<float>` | Copy | `float32 (2,)` | Too small to benefit from zero-copy |
| `DigitalEventSeries` | Event times | Copy | `int64` | Non-contiguous storage |
| `DigitalIntervalSeries` | Interval pairs | Copy | `int64 (N,2)` | Non-contiguous storage |
| `MediaData` | `getRawData8/32(frame)` | Copy | `uint8/float32` | Per-frame, returned by const ref to vector |

### 9.2 Write Path (Python → C++)

Zero-copy writes are **not safe** for the general case because:
1. Python/NumPy owns the memory — if the Python object is GC'd, the C++ pointer dangles.
2. The observer pattern expects C++ to own data.

**Instead:** Accept NumPy arrays by copying into new C++ objects:
```python
# User code:
filtered = np.array(filtfilt(b, a, arr), dtype=np.float32)
ts = AnalogTimeSeries(filtered, time_indices)   # copies data
dm.setData("filtered", ts, "default")           # DataManager takes ownership via shared_ptr
```

For large data, consider a `move` variant that transfers ownership of a NumPy buffer to C++
via `py::buffer_info`, but this requires careful lifetime management and should be Phase 7.

### 9.3 Implementation: `numpy_utils.hpp`

```cpp
#pragma once
#include <pybind11/numpy.h>
#include <span>

namespace wt::python {

// Zero-copy: create a read-only NumPy array viewing a C++ span.
// The `owner` ref-counts the C++ object to keep the memory alive.
template<typename T>
py::array_t<T> span_to_numpy(std::span<T const> data, py::object owner) {
    auto result = py::array_t<T>(
        {static_cast<py::ssize_t>(data.size())},  // shape
        {sizeof(T)},                                // strides
        data.data(),                                // data pointer
        owner                                       // prevent GC of C++ object
    );
    // Mark read-only
    reinterpret_cast<PyArrayObject*>(result.ptr())->flags &= ~NPY_ARRAY_WRITEABLE;
    return result;
}

// Copy: create a NumPy array from a C++ span (data is copied)
template<typename T>
py::array_t<T> span_to_numpy_copy(std::span<T const> data) {
    auto result = py::array_t<T>(static_cast<py::ssize_t>(data.size()));
    std::copy(data.begin(), data.end(), result.mutable_data());
    return result;
}

// Copy NumPy array into a std::vector
template<typename T>
std::vector<T> numpy_to_vector(py::array_t<T, py::array::c_style | py::array::forcecast> arr) {
    auto buf = arr.request();
    auto * ptr = static_cast<T*>(buf.ptr);
    return std::vector<T>(ptr, ptr + buf.size);
}

} // namespace wt::python
```

### 9.4 Lifetime Safety

The key to zero-copy reads is ensuring the C++ object stays alive while Python holds a
view. This is handled by passing the `shared_ptr` holder as the `owner` parameter to
the NumPy array constructor. pybind11 ref-counts the shared_ptr, preventing deallocation.

```cpp
// Example in bind_analog.cpp:
cls.def_property_readonly("values", [](std::shared_ptr<AnalogTimeSeries> const & self) {
    auto span = self->getAnalogTimeSeries();
    // Pass self as owner — prevents GC while array exists
    return wt::python::span_to_numpy(span, py::cast(self));
});
```

---

## 10. Threading & Safety

### 10.1 GIL Management

| Scenario | GIL State | Notes |
|----------|-----------|-------|
| Python code running | Held | Normal execution |
| C++ binding code | Released where safe | Use `py::gil_scoped_release` in long C++ operations |
| UI event handling | Not held | Qt event loop runs without GIL |
| DataManager mutation from Python | Held | Must be on main thread |

### 10.2 Thread Model (Phase 1-5)

**Single-threaded:** All Python execution is synchronous on the main thread. The UI
blocks during execution. This is simple and correct.

### 10.3 Thread Model (Phase 7 - Async)

**Worker thread with marshalling:**

```
Main Thread                Worker Thread
     │                          │
     │──── start script ───────►│
     │                          │ acquire GIL
     │                          │ execute Python
     │                          │ release GIL
     │◄─── results ────────────│
     │                          │
     │ update DataManager       │
     │ notify observers         │
```

DataManager mutations from the worker thread must use:
```cpp
QMetaObject::invokeMethod(qApp, [&dm, key, data, time_key]() {
    dm->setData(key, data, time_key);
}, Qt::BlockingQueuedConnection);
```

### 10.4 Observer Safety

The `ObserverData` pattern uses `std::function` callbacks and is **not thread-safe**.
All observer notifications must happen on the main thread. This is naturally the case
in Phase 1-5 (single-threaded). For Phase 7, use the marshalling pattern above.

---

## 11. Testing Strategy

### 11.1 Unit Tests (`tests/python_bindings/`)

| Test File | Coverage |
|-----------|----------|
| `PythonEngine.test.cpp` | Interpreter boot, execute, stdout/stderr capture, namespace persistence (14 tests, Phase 1) |
| `bindings.test.cpp` | All Phase 2 bindings: geometry, timeframe, entity, analog, digital event/interval, line, mask, point, tensor, DataManager (24 tests, Phase 2) |
| `PythonBridge.test.cpp` | Bridge integration: expose dm, setData/getData round-trips, observer notifications, importNewData, exposeData/TimeFrame, error recovery, e2e workflows (27 tests, Phase 3) |
| `test_venv.cpp` | Venv activation, sys.path modification, import check |

### 11.2 Integration Tests

- Load a real dataset → run a Python script that filters analog data → verify filtered
  data appears in DataManager with correct time frame.
- Create data in Python (events, intervals) → verify observable by C++ widgets.
- Execute faulty script → verify error is captured, no crash, interpreter still usable.

### 11.3 Benchmark Tests

- Zero-copy array access latency (should be < 1μs for pointer wrapping).
- Roundtrip: C++ → NumPy → scipy.filtfilt → new C++ object for 1M-sample time series.
- Compare with `execute_file` for a realistic analysis script.

---

## 12. Open Questions & Risks

### 12.1 Design Decisions (Resolved)

| # | Question | Decision |
|---|----------|----------|
| 1 | **Single interpreter vs. sub-interpreters?** | **Single interpreter.** Simpler, all libraries work. Sub-interpreters (PEP 684) have limited ecosystem support. |
| 2 | **Module import: embedded vs. external?** | **Embedded only** — compile bindings into the executable. No standalone `.so`/`.pyd` for external Python use. |
| 3 | **Syntax highlighting library?** | **Custom `QSyntaxHighlighter`.** Minimal dependency; Python-only highlighting is manageable in-house. |
| 4 | **Editor vs. external IDE?** | **Built-in editor for quick work**, with "Open in External Editor" button for complex scripts. |
| 5 | **Matplotlib backend?** | **Phase 1–5:** No special support — matplotlib opens a separate window. **Phase 7:** Embed `FigureCanvasQTAgg` in the widget. |
| 6 | **How to handle `input()`?** | **Disallow** — raise an error. Blocking `input()` complicates the event loop. |

### 12.2 Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **GIL contention** blocks UI during long scripts | High — UI freezes | Phase 7 async execution; short-term: document that long scripts will block UI |
| **Python version mismatch** between embedded and venv | Medium — import failures | Strict version validation on venv activation |
| **Memory leaks** from C++ ↔ Python ref-count cycles | Medium | Use weak references where possible; ensure shared_ptr holders are properly released |
| **pybind11 ABI compatibility** with user-installed packages (e.g. numpy built with different compiler) | Medium | Pin pybind11 version; test with common packages; document supported configurations |
| **Platform differences** in Python paths and venv layout | Low-Medium | Abstract venv discovery behind platform-specific helpers (already cross-platform via CMake) |
| **Observer thread safety** if async execution is added | High — data corruption | Strict main-thread marshalling for all DataManager mutations (Phase 7) |

---

## Suggested Implementation Order

```
Phase 1 (Interpreter)      ████████████████████  COMPLETE
Phase 2 (Bindings)          ████████████████████  COMPLETE
Phase 3 (Bridge)            ████████████████████  COMPLETE
Phase 4 (Widget UI)         ░░░░░░░░░░░░░░██████  ~3 weeks  ← NEXT
Phase 5 (Script Mgmt)       ░░░░░░░░░░░░░░░░░░██  ~1 week
Phase 6 (Venv)              ░░░░░░░░░░░░░░░░░░██  ~1 week
Phase 7 (Advanced)          ░░░░░░░░░░░░░░░░░░░░  ongoing
```

Phases 1–3 can be developed and tested without any Qt UI (pure C++ + pybind11).
Phase 4 builds the UI on top of the working engine. Phases 5–6 are incremental
enhancements. Phase 7 items are independent and can be tackled as-needed.

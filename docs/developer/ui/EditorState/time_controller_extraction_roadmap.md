# TimeController Extraction Roadmap

## Motivation

`EditorRegistry` currently combines temporal state management, data focus,
selection context, operation context, and widget factory registration into a
single Qt-dependent class. The Command architecture (`ICommand`, `CommandContext`)
is deliberately Qt-free — it links only `DataManager` and `TimeFrame`.

This creates a problem: commands that need to navigate time (e.g., `AdvanceFrame`
for triage hotkey sequences) cannot access `EditorRegistry` without pulling in
`Qt6::Core` and `Qt6::Widgets` as transitive dependencies.

A previous code review flagged that a separate `TimeController` should be
extracted from `EditorRegistry`. This roadmap executes that refactoring.

## Goal

Extract the temporal state management from `EditorRegistry` into a standalone,
Qt-free `TimeController` class that:

- Owns the current `TimePosition` and active `TimeKey`
- Provides the same read/write API as today
- Uses a simple `std::function` callback for change notification (not
  `ObserverData` — there is only one consumer: the `EditorRegistry` bridge)
- Can be placed on `CommandContext` alongside `DataManager*`
- Depends only on the `TimeFrame` library

`EditorRegistry` retains ownership of `TimeController` and bridges its callback
to Qt signals so all existing widget code continues to work unchanged.

## Progress

| Phase | Status |
|-------|--------|
| **Phase 1** — `TimeController` library + tests | **Complete** — builds cleanly; `test_timecontroller` exercises state, callbacks, re-entrancy guard, equality short-circuit, and `setActiveTimeKey`. |
| **Phase 2** — `EditorRegistry` owns and delegates | Not started |
| **Phase 3** — `CommandContext` + commands | Not started |

Phase 1 also added a short developer note at `docs/developer/TimeController/TimeController.qmd` (linked from the TimeFrame section in `docs/_quarto.yml`) and a one-line architecture entry in `AGENTS.md` / `.github/copilot-instructions.md`.

## Current State

### Time-Related API on EditorRegistry

| Method | Pure C++? | Callers |
|--------|-----------|---------|
| `setCurrentTime(TimePosition const&)` | Body is pure C++ + `emit` | TimeScrollBar (2), 13 widget registrations |
| `setCurrentTime(TimeFrameIndex, shared_ptr<TimeFrame>)` | Delegates to above | 0 direct callers |
| `setCurrentTime(int64_t)` | Deprecated | 0 callers found |
| `currentPosition() const` | Yes | TimeScrollBar (3), Export_Video, Tongue/Whisker/DataInspector registrations |
| `currentTimeIndex() const` | Yes | 0 callers (unused) |
| `currentTimeFrame() const` | Yes | 0 callers (unused) |
| `setActiveTimeKey(TimeKey)` | Body is pure C++ + `emit` | 0 callers (dead code) |
| `activeTimeKey() const` | Yes | 0 callers (dead code) |

### Signals

| Signal | Connections |
|--------|------------|
| `timeChanged(TimePosition)` | 9 unique widgets (some connected twice via dual factory paths) |
| `activeTimeKeyChanged(TimeKey, TimeKey)` | 0 connections (dead code) |

### Private State (time-related)

- `TimePosition _current_position`
- `TimeKey _active_time_key{TimeKey("time")}`
- `bool _time_update_in_progress{false}`

### CMake

`EditorState` links `Qt6::Core`, `Qt6::Widgets`, `reflectcpp`, `TimeFrame`.

---

## Phase 1 — Create TimeController Library

**Status:** complete (library is integrated in the tree; `EditorRegistry` still uses its own fields until Phase 2).

### 1.1 New Library: `TimeController`

**Location:** `src/TimeController/`

**CMake target:** `TimeController` (static library, alias
`WhiskerToolbox::TimeController`)

**Dependencies:** `TimeFrame` only (Qt-free)

**Public API:**

```cpp
class TimeController {
public:
    using TimeChangedCallback = std::function<void(TimePosition const&)>;
    using TimeKeyChangedCallback = std::function<void(TimeKey new_key, TimeKey old_key)>;

    void setCurrentTime(TimePosition const& position);
    void setCurrentTime(TimeFrameIndex index, std::shared_ptr<TimeFrame> tf);

    [[nodiscard]] TimePosition currentPosition() const;
    [[nodiscard]] TimeFrameIndex currentTimeIndex() const;
    [[nodiscard]] std::shared_ptr<TimeFrame> currentTimeFrame() const;

    void setActiveTimeKey(TimeKey key);
    [[nodiscard]] TimeKey activeTimeKey() const;

    void setOnTimeChanged(TimeChangedCallback cb);
    void setOnTimeKeyChanged(TimeKeyChangedCallback cb);

private:
    TimePosition _current_position;
    TimeKey _active_time_key{TimeKey("time")};
    bool _time_update_in_progress{false};
    TimeChangedCallback _on_time_changed;
    TimeKeyChangedCallback _on_time_key_changed;
};
```

The re-entrancy guard logic (`_time_update_in_progress`) moves here verbatim
from `EditorRegistry::setCurrentTime()`.

### 1.2 Tests

Catch2 tests in `tests/TimeController/`:
- Set time, verify `currentPosition()` reflects it
- Verify callback fires on `setCurrentTime()`
- Verify re-entrancy guard prevents recursive calls
- Verify equality short-circuit (same position → no callback)
- Verify `setActiveTimeKey()` callback

**Delivered:** `tests/TimeController/TimeController.test.cpp` implements the above via `test_timecontroller` (CMake: `tests/TimeController/CMakeLists.txt`), including the `setCurrentTime(TimeFrameIndex, shared_ptr<TimeFrame>)` convenience overload.

---

## Phase 2 — Integrate into EditorRegistry

### 2.1 EditorRegistry Owns TimeController

Add `std::unique_ptr<TimeController>` (or value member) to `EditorRegistry`.
Wire the callback in the constructor:

```cpp
_time_controller = std::make_unique<TimeController>();
_time_controller->setOnTimeChanged([this](TimePosition const& pos) {
    emit timeChanged(pos);
});
_time_controller->setOnTimeKeyChanged([this](TimeKey nk, TimeKey ok) {
    emit activeTimeKeyChanged(nk, ok);
});
```

### 2.2 Delegate Time Methods

`EditorRegistry`'s time methods become thin wrappers:

```cpp
void EditorRegistry::setCurrentTime(TimePosition const& pos) {
    _time_controller->setCurrentTime(pos);
}
TimePosition EditorRegistry::currentPosition() const {
    return _time_controller->currentPosition();
}
// etc.
```

All existing callers (TimeScrollBar, 13 widget registrations, 9 signal
connections) continue to compile and work unchanged — they still call
`EditorRegistry` methods and connect to `EditorRegistry` signals.

### 2.3 Expose TimeController Pointer

Add accessor:

```cpp
[[nodiscard]] TimeController * timeController() const;
```

This is used by `MainWindow` (or wherever `CommandContext` is built) to pass
the non-owning pointer.

### 2.4 Clean Up Dead Code

The exploration found several unused APIs:
- `currentTimeIndex()` — 0 callers
- `currentTimeFrame()` — 0 callers
- `activeTimeKey()` / `setActiveTimeKey()` — 0 callers
- `activeTimeKeyChanged` signal — 0 connections
- `setCurrentTime(int64_t)` deprecated overload — 0 callers

These can either be kept on `TimeController` for future use or removed. If
kept, mark the EditorRegistry wrappers as deprecated to steer callers toward
`timeController()` directly for new code.

---

## Phase 3 — Add TimeController to CommandContext

### 3.1 Extend CommandContext

Add an optional `TimeController *` field to `CommandContext`:

```cpp
struct CommandContext {
    DataManager * data_manager = nullptr;
    TimeController * time_controller = nullptr;  // new
    std::map<std::string, std::string> runtime_variables;
    // ...
};
```

`Commands` CMake target adds `TimeController` as a dependency (Qt-free, so
no contamination).

### 3.2 Wire in Execution Sites

Wherever `CommandContext` is built, populate `time_controller`:

- **TriageSession::commit()** — `ctx.time_controller = time_controller_ptr`
- **KeymapManager command-action execution** (Phase 2 of triage plan) — same
- **Any future headless/test execution** — can pass a standalone
  `TimeController` with no Qt

### 3.3 Create AdvanceFrame Command

With `TimeController` on `CommandContext`, `AdvanceFrame` is straightforward:

```cpp
CommandResult execute(CommandContext const& ctx) override {
    auto pos = ctx.time_controller->currentPosition();
    auto new_index = TimeFrameIndex(pos.index.getValue() + _params.delta);
    ctx.time_controller->setCurrentTime(
        TimePosition{new_index, pos.time_frame});
    return CommandResult::success();
}
```

This is a real `ICommand` — recordable, composable in sequences, no Qt
dependency.

---

## Verification

1. **Unit tests (Phase 1):** ✅ `TimeController` standalone tests — state
   management, callback, re-entrancy guard (`ctest` discovers cases from `test_timecontroller`).
2. **Build (Phase 2):** Full build succeeds with zero changes to any widget
   code. All existing `EditorRegistry` callers and signal connections compile.
3. **Existing test suite (Phase 2):** `ctest --preset linux-clang-release`
   passes — no regressions.
4. **Command tests (Phase 3):** `AdvanceFrame` command test with a standalone
   `TimeController` (no Qt, no EditorRegistry).
5. **Integration (Phase 3):** JSON sequence "SetEventAtTime + AdvanceFrame" via
   `executeSequence()` with a `CommandContext` containing both `DataManager` and
   `TimeController`.

---

## Files to Create

| File | Phase | Purpose | Status |
|------|-------|---------|--------|
| `src/TimeController/TimeController.hpp` | 1 | Qt-free time state class | Done |
| `src/TimeController/TimeController.cpp` | 1 | Implementation | Done |
| `src/TimeController/CMakeLists.txt` | 1 | Static library, depends on `TimeFrame` | Done |
| `tests/TimeController/TimeController.test.cpp` | 1 | Unit tests | Done |
| `tests/TimeController/CMakeLists.txt` | 1 | Test binary | Done |

## Files to Modify

| File | Phase | Change | Status |
|------|-------|--------|--------|
| `src/CMakeLists.txt` | 1 | Add `TimeController` subdirectory | Done |
| `tests/CMakeLists.txt` | 1 | Add `TimeController` test subdirectory | Done |
| `src/WhiskerToolbox/EditorState/EditorRegistry.hpp` | 2 | Add `unique_ptr<TimeController>`, delegate methods | Pending |
| `src/WhiskerToolbox/EditorState/EditorRegistry.cpp` | 2 | Wire callback → signal bridge, delegate implementations | Pending |
| `src/WhiskerToolbox/EditorState/CMakeLists.txt` | 2 | Add `TimeController` dependency | Pending |
| `src/Commands/Core/CommandContext.hpp` | 3 | Add `TimeController *` field | Pending |
| `src/Commands/CMakeLists.txt` | 3 | Add `TimeController` dependency | Pending |

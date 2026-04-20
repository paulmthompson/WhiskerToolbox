# Optimized Manual Contact Triage Plan

## Motivation

When triaging videos with hundreds of thousands of frames for contact vs
no-contact events, two pieces of information must be tracked per frame:

1. **Contact label** — whether the frame is contact or no-contact.
2. **Tracked status** — whether the frame has been reviewed at all.

The distinction matters: an untracked frame is *unlabeled*, while a tracked
non-contact frame is a deliberate negative. Because videos are so large,
reducing playback from 25 FPS to ~1 FPS for manual labeling creates enormous
time costs. ML-based classification (e.g., HMM) helps but introduces a
false-positive/false-negative cleanup burden that can negate the savings.

This plan optimizes the manual triage workflow across five phases:

| Phase | Goal | Dependencies | Status |
|-------|------|--------------|--------|
| 1 | Atomic frame-level Commands | None | **Done** |
| 2 | Hotkey → Command Sequence bridge | Phase 1 | **Done** |
| 3 | Sub-25 FPS playback | None (parallel) | **Done** |
| 4 | Dynamic frame filter on TimeScrollBar | None (parallel) | **Done** |
| 5 | Adaptive FPS from ML confidence | Phases 3 & 4 | Not started |

---

## Current State

### What Exists

| Capability | Location |
|---|---|
| Frame-level flip (contact toggle) | `DigitalIntervalSeries::setEventAtTime()` + `DigitalIntervalSeriesInspector::_flipIntervalButton()` |
| Interval create / delete / extend / merge | `DigitalIntervalSeriesInspector` |
| Triage mark / commit / recall state machine | `TriageSession` |
| Tracked regions stored as intervals | `tracked_regions` key in `TriageSessionState` |
| Keymap system with scopes + adapter dispatch | `KeymapSystem` (`AlwaysRouted`, `EditorFocused`, `Global`) |
| Command architecture with sequencing | `Commands/Core` (`CommandSequenceDescriptor`, `executeSequence`) |
| Atomic triage commands (`ICommand`) | `SetEventAtTime`, `FlipEventAtTime`, `AdvanceFrame` in `src/Commands/`; registered in `register_core_commands.cpp` |
| `TimeController` on `CommandContext` | `src/Commands/Core/CommandContext.hpp` — optional pointer for navigation commands |
| Hotkey → command sequences | `KeymapManager::registerCommandAction()` + optional `command_sequence` on `KeyActionDescriptor`; `KeymapCommandBridge` builds `CommandContext` and calls `executeSequence()`; `MainWindow` sets `KeymapManager::setDataManager()`; `TimeController *` from `EditorRegistry::timeController()` |
| Default contact triage chords (global) | `TriageSessionWidgetRegistration.cpp` — **F** / **D** / **G** (`triage.contact_mark_tracked_advance`, `triage.contact_flip_tracked_advance`, `triage.tracked_advance`) |
| Tests: frame commands + sequences | `src/Commands/SetEventAtTime.test.cpp`, `src/Commands/AdvanceFrame.test.cpp` |
| Tests: keymap command bridge | `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` — `registerCommandAction`, `executeCommandSequenceFromRegistry` |
| Timeline arrow keys (±1, ±N jump) | `TimeScrollBar` + `KeymapSystem` |
| Playback 0.5–200 FPS (preset ladder) | `TimeScrollBar` + `time_scroll_bar::timerPeriodMsForTargetFps()` — QTimer interval `1000/target_fps` ms (clamped), **one frame per tick**; rewind / fast-forward step discrete presets (`TimeScrollBarPlayback.hpp`) |
| Persisted playback rate | `TimeScrollBarStateData::target_fps` (`float`); `TimeScrollBarState::setTargetFps()` snaps to presets; workspace JSON migration from legacy `play_speed` (`25 × multiplier`) in `TimeScrollBarState::fromJson()` |
| Doc: timeline playback | [TimeScrollBar developer note](../ui/TimeScrollBar/index.qmd) |

### What is Missing

| Capability | Notes |
|---|---|
| Adaptive FPS from ML confidence | No infrastructure (Phase 5) |

---

## Phase 1 — Atomic Frame-Level Commands

**Status: complete.** Three commands in `src/Commands/` are registered as composable
primitives for triage hotkey sequences (see also
[Concrete Commands](../DataManager/Commands/commands.qmd)). No dependencies on other phases.

**Layout:** Each command uses a paired `.hpp` / `.cpp` at `src/Commands/<Name>.{hpp,cpp}`
(not per-command subdirectories). `TimeController *` on `CommandContext` was introduced
with `AdvanceFrame` so navigation stays inside `executeSequence()` like other commands.

### 1.1 `SetEventAtTime`

- **Parameters:** `data_key` (string), `time` (int64, supports `${current_frame}`), `event` (bool)
- **Action:** Calls `DigitalIntervalSeries::setEventAtTime(TimeFrameIndex(time), event)` on the target data object.
- **Category:** `data_mutation`

This is the primitive for both "mark tracked" (`event=true` on the tracked-regions key) and "mark contact" (`event=true` on the contact key).

### 1.2 `FlipEventAtTime`

- **Parameters:** `data_key` (string), `time` (int64, supports `${current_frame}`)
- **Action:** Reads `hasIntervalAtTime()`, then calls `setEventAtTime()` with the opposite value.
- **Category:** `data_mutation`

This is the primitive for the single-key toggle hotkey.

### 1.3 `AdvanceFrame`

- **Parameters:** `delta` (`int64_t`, default `1`) — signed frame step
- **Action:** Uses `CommandContext::time_controller` (`TimeController`) to advance the current time position.
- **Category:** `navigation` (registry metadata; same intent as “UI navigation”)

`TimeController` is a Qt-free class extracted from `EditorRegistry`.
It depends only on `TimeFrame`, so adding it to `CommandContext` preserves the
Qt-free boundary of the Command architecture. `EditorRegistry` retains ownership
of `TimeController` and bridges its callbacks to Qt signals — all existing
widget code continues to work unchanged.

### 1.4 Composite Example

"Mark contact + mark tracked + next frame" is not a new command class — it is
a `CommandSequenceDescriptor`:

```json
{
  "name": "mark_contact_and_advance",
  "commands": [
    { "command_name": "SetEventAtTime",
      "parameters": { "data_key": "contact", "time": "${current_frame}", "event": true }},
    { "command_name": "SetEventAtTime",
      "parameters": { "data_key": "tracked_regions", "time": "${current_frame}", "event": true }},
    { "command_name": "AdvanceFrame",
      "parameters": { "delta": 1 }}
  ]
}
```

---

## Phase 2 — Hotkey → Command Sequence Bridge

**Status: complete.** `KeymapSystem` can register actions whose payload is a
`CommandSequenceDescriptor` and run `executeSequence()` on key press, without
going through `TriageSession::commit()`.

### 2.1 `registerCommandAction()` on `KeymapManager`

Implemented signature (includes `KeyActionScope`, same as `registerAction`):

```
registerCommandAction(action_id, display_name, category,
                      scope, default_binding, CommandSequenceDescriptor)
```

`KeyActionDescriptor` carries an optional `command_sequence`. When present,
`KeymapManager::eventFilter()` dispatches via `KeymapSystem::executeCommandSequenceFromRegistry()`
instead of widget adapters.

That helper builds `CommandContext` with:

1. **`std::shared_ptr<DataManager>`** — from `KeymapManager::setDataManager()`, called in `MainWindow` with the app-wide manager (not stored on `EditorRegistry`).
2. **`TimeController *`** — from `EditorRegistry::timeController()` when the registry pointer is non-null.
3. **Runtime variables** — `current_frame` from `TimeController::currentTimeIndex()`, `current_time_key` from `activeTimeKey().str()`.

Then `executeSequence()` runs as usual; `CommandContext` stays Qt-free.

### 2.2 `TimeController` on `CommandContext`

**Done in Phase 1.** Phase 2 passes the registry’s `TimeController` into the context via the bridge.

### 2.3 User-Configurable Hotkeys

The `KeybindingEditor` lists registered command-actions like any other action.
Defaults shipped for contact triage (**Global** scope, category **Contact Triage**):

| Key | Action ID | Sequence |
|-----|-----------|----------|
| `F` | `triage.contact_mark_tracked_advance` | SetEventAtTime(contact) + SetEventAtTime(tracked_regions) + AdvanceFrame(1) |
| `D` | `triage.contact_flip_tracked_advance` | FlipEventAtTime(contact) + SetEventAtTime(tracked_regions) + AdvanceFrame(1) |
| `G` | `triage.tracked_advance` | SetEventAtTime(tracked_regions) + AdvanceFrame(1) |
| `Right` | `timeline.next_frame` | AdvanceFrame(1) *(existing timeline binding — unchanged)* |

Registration: `TriageSessionWidgetRegistration.cpp` (alongside Mark/Commit/Recall).

---

## Phase 3 — Sub-25 FPS Playback

**Status: complete.** Independent of Phases 1–2; developed in parallel.

### 3.1 Variable Timer Period (implemented)

- `TimeScrollBar` keeps `float _target_fps` (default 25). Playback uses
  `time_scroll_bar::timerPeriodMsForTargetFps(_target_fps)` for the QTimer
  interval (milliseconds rounded from `1000 / target_fps`, clamped to a 1 ms minimum and a large upper bound for safety).
- Each timer tick advances **exactly one frame** (`_vidLoop()`), regardless of
  target FPS. Rates above ~1000 FPS are limited by the 1 ms minimum interval.

### 3.2 Discrete FPS Presets (implemented)

Rewind / FastForward step the same ladder used for snapping persisted values:

```
0.5  →  1  →  2  →  5  →  10  →  25  →  50  →  100  →  200
```

The `fps_label` shows the current preset (integers without decimals; `0.5` as
one decimal). `TimeScrollBarStateData` stores `target_fps`; `TimeScrollBarState`
snaps with `snapTargetFpsToPreset()`. Legacy workspace JSON that only had
integer `play_speed` is migrated to `target_fps = 25 × play_speed` before
deserialization.

### 3.3 Why This Matters

At 0.5 FPS, the user sees each frame for 2 seconds — enough to make a labeling
decision on challenging sections without stopping playback entirely.

---

## Phase 4 — Dynamic Frame Filter on TimeScrollBar

Independent of Phases 1–2; can be developed in parallel.

### 4.1 `FrameFilter` Interface

```cpp
class FrameFilter {
public:
    virtual ~FrameFilter() = default;
    virtual bool shouldSkip(TimeFrameIndex index) const = 0;
};
```

Located in `src/WhiskerToolbox/TimeScrollBar/` initially (UI concern); extract
to core later if other components need it.

### 4.2 `DataKeyFrameFilter`

Concrete implementation backed by any `DigitalIntervalSeries` key (not hardcoded
to `tracked_regions`):

- Constructor takes `data_key` + `DataManager` reference.
- `shouldSkip()` returns `true` if `hasIntervalAtTime(index)` is `true` (frame
  is inside the interval series → already tracked → skip).
- Filter direction is invertible (skip tracked vs. skip untracked).

### 4.3 Integration into TimeScrollBar

- **`changeScrollBarValue()`**: After computing the next frame, if a filter is
  active, scan forward/backward to find the next non-skipped frame. Include a
  max-scan limit to prevent infinite loops on fully-tracked videos.
- **`_vidLoop()`**: Same skip logic during playback.
- **UI**: Checkbox "Skip tracked frames" + data key selector combo box in
  TimeScrollBar (or a small toolbar row).

### 4.4 Dynamic Behavior

Because `DigitalIntervalSeries` notifies via `ObserverData`, the filter
responds in real time. When the user marks a frame as tracked (via a Phase 2
hotkey), that frame is immediately excluded from navigation. During playback,
tracked frames are skipped seamlessly — the user only sees untracked frames.

As triage progresses, the remaining frame count shrinks, accelerating the
workflow.

---

## Phase 5 — Adaptive FPS from ML Confidence (Future)

Depends on Phases 3 and 4. The idea: instead of a fixed playback speed, let the
HMM's *uncertainty* control how fast the video plays. High-confidence regions
fly by; ambiguous regions near state transitions slow down automatically.

### 5.1 The Right Signal: Transition Probability

The HMM (via mlpack) already provides:

| Output | Method | Shape |
|--------|--------|-------|
| Per-frame posterior $\gamma_t(i) = P(s_t = i \mid o_{1:T})$ | `predictProbabilities()` / `predictProbabilitiesPerSequence()` | `(num_states × T)` |
| Learned transition matrix $A$ | `getTransitionMatrix()` | `(num_states × num_states)` |
| Forward/backward variables $\alpha_t$, $\beta_t$ | Computed internally in `predictProbabilitiesPerSequence()` | Not returned |

For adaptive FPS, the ideal signal is the **per-frame transition probability**:

$$P(\text{transition at } t) = P(s_{t+1} \neq s_t \mid o_{1:T}) = 1 - \sum_i \xi_t(i, i)$$

where the pairwise posterior is:

$$\xi_t(i, j) = P(s_t = i,\; s_{t+1} = j \mid o_{1:T})$$

This requires the $\xi$ matrix, which is not currently exposed but can be
computed from the forward/backward variables that are already calculated.

### 5.2 Option A — Approximate (No Code Changes to MLCore)

Use existing marginal posteriors to approximate:

$$P(\text{transition at } t) \approx 1 - \sum_i \gamma_t(i) \cdot \gamma_{t+1}(i)$$

This is an **upper bound** — it overestimates transition probability because it
ignores temporal correlations between adjacent frames. However, for FPS
modulation this may actually be desirable: it errs on the side of slowing down
in ambiguous regions.

**Workflow:**
1. Run `predictProbabilities()` → get $\gamma$ matrix.
2. Compute the approximation for each frame pair.
3. Store as an `AnalogTimeSeries`.
4. Feed to `AdaptiveFPSController`.

### 5.3 Option B — Exact via $\xi$ (Small Extension to MLCore)

The forward/backward variables (`forwardLog`, `backwardLog`, `logTransition`,
`logProbs`) are already computed inside `predictProbabilitiesPerSequence()` at
`src/MLCore/models/supervised/HiddenMarkovModelOperation.cpp` lines 516–576.

A new method `computeTransitionPosteriors()` would compute:

$$\xi_t(i, j) = \frac{\alpha_t(i) \cdot a_{ij} \cdot b_j(o_{t+1}) \cdot \beta_{t+1}(j)}{P(o_{1:T})}$$

Then derive the scalar per-frame transition probability. This is exact and adds
minimal computation on top of the existing Forward-Backward pass.

Alternatively, mlpack's `HMM::Estimate()` has a three-argument overload
`Estimate(obs, stateProb, stateTransProb)` that returns $\xi$ directly. This
works for the unconstrained case; the constrained Forward-Backward
(`predictProbabilitiesPerSequence`) would need its own $\xi$ computation.

### 5.4 `AdaptiveFPSController`

A new class that modulates `target_fps` based on a per-frame signal:

- **Input:** An `AnalogTimeSeries` (or `TensorData` column) keyed by
  `TimeFrameIndex`, containing transition probability (or any 0–1 confidence
  score).
- **Mapping function:** `confidence → fps`. Example: linear mapping where
  $P(\text{transition}) = 0$ → `max_fps`, $P(\text{transition}) = 1$ →
  `min_fps`.
- **Parameters:** `min_fps` (e.g., 0.5), `max_fps` (e.g., 100),
  `confidence_key`, `column_index`, `mapping_curve`.
- **Integration:** On each frame advance in `TimeScrollBar`, the controller
  queries the signal at the new frame and adjusts the timer period.

### 5.5 Adaptive FPS UI

- Dropdown in TimeScrollBar: "Speed mode: Fixed / Adaptive"
- When Adaptive: select confidence data key, set min/max FPS
- Optional: overlay confidence as a colored band on the scrollbar track

---

## Files to Modify

| File | Phase | Change |
|------|-------|--------|
| `src/Commands/Core/CommandContext.hpp` | 1 | Optional `TimeController *` (done) |
| `src/Commands/CMakeLists.txt` | 1 | Sources + link `WhiskerToolbox::TimeController` (done) |
| `src/Commands/register_core_commands.cpp` | 1 | Register `SetEventAtTime`, `FlipEventAtTime`, `AdvanceFrame` (done) |
| `tests/Commands/CMakeLists.txt` | 1 | Add `SetEventAtTime.test.cpp` to `test_commands` (done) |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` / `.cpp` | 2 | `registerCommandAction()`, `setDataManager()`, dispatch to bridge (**done**) |
| `src/WhiskerToolbox/KeymapSystem/KeyAction.hpp` | 2 | Optional `command_sequence` on descriptor (**done**) |
| `src/WhiskerToolbox/KeymapSystem/KeymapCommandBridge.hpp` / `.cpp` | 2 | `executeCommandSequenceFromRegistry(dm, registry, seq)` (**done**) |
| `src/Commands/Core/SequenceExecution.hpp` | 2 | Include `ICommand.hpp` so `SequenceResult` destructs when used from UI TU (**done**) |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | 2 | `KeymapManager::setDataManager(_data_manager)` (**done**) |
| `src/WhiskerToolbox/TriageSession_Widget/TriageSessionWidgetRegistration.cpp` | 2 | Default **F** / **D** / **G** contact triage chords (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` / `.cpp` | 3, 4 | Phase **3 done**: `_target_fps`, timer interval, preset stepping, 1 frame/tick; Phase **4 done**: `setFrameFilter()`, `_frame_filter`, filter integration in `_vidLoop()` + `changeScrollBarValue()`, `_rebuildFrameFilter()` (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBarState.hpp` / `.cpp` | 3 | `targetFps` / `setTargetFps`, `fromJson` migration from `play_speed` (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBarStateData.hpp` | 3 | `target_fps` float (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.ui` | 4 | `skip_tracked_checkbox` + `filter_key_combobox` row (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBarRegistration.cpp` | 3 | No keymap changes required for FPS (**n/a**) |
| `tests/fuzz/unit/EditorState/fuzz_*_roundtrip.cpp`, `fuzz_editor_state_from_json.cpp`, `fuzz_state_data_from_garbage.cpp` | 3 | `target_fps` / preset-index fuzzing; `TimeScrollBarMigratesLegacyPlaySpeedJson` (**done**) |
| `docs/developer/ui/TimeScrollBar/index.qmd`, `docs/_quarto.yml` | 3 | Playback + persistence overview (**done**) |
| `src/MLCore/models/supervised/HiddenMarkovModelOperation.hpp` / `.cpp` | 5 (Option B) | Expose $\xi$ computation |

## Files to Create

| File | Phase | Purpose |
|------|-------|---------|
| `src/Commands/SetEventAtTime.hpp` / `SetEventAtTime.cpp` | 1 | `SetEventAtTime` command (**done**) |
| `src/Commands/FlipEventAtTime.hpp` / `FlipEventAtTime.cpp` | 1 | `FlipEventAtTime` command (**done**) |
| `src/Commands/AdvanceFrame.hpp` / `AdvanceFrame.cpp` | 1 | `AdvanceFrame` command (**done**) |
| `src/Commands/SetEventAtTime.test.cpp` | 1 | Catch2 tests for interval commands + triage sequence (**done**) |
| `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` | 2 | `registerCommandAction` + bridge smoke test (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBarPlayback.hpp` | 3 | Preset table, snap, timer ms helper (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/FrameFilter.hpp` / `.cpp` | 4 | `FrameFilter` interface + `DataKeyFrameFilter` + `frame_filter::scanToNextNonSkipped` (**done**) |
| `tests/WhiskerToolbox/TimeScrollBar/test_frame_filter.cpp` | 4 | Catch2 tests for `FrameFilter` and scan utility (**done**) |
| `tests/WhiskerToolbox/TimeScrollBar/CMakeLists.txt` | 4 | Test executable for `test_frame_filter` (**done**) |
| `src/WhiskerToolbox/TimeScrollBar/AdaptiveFPSController.hpp` / `.cpp` | 5 | FPS modulation controller |

---

## Verification

1. **Catch2 tests for new commands** — **Done** (`SetEventAtTime.test.cpp`): `SetEventAtTime`
   and `FlipEventAtTime` with `DigitalIntervalSeries`; flip toggles; `${current_frame}`
   substitution; JSON sequence “mark contact + mark tracked + `AdvanceFrame`” via
   `executeSequence()`.
2. **Catch2 test for `FrameFilter`** — **Done** (`tests/WhiskerToolbox/TimeScrollBar/test_frame_filter.cpp`): `shouldSkip()` with known intervals; inverted mode; boundary cases (first frame, last frame, empty series, fully tracked); `scanToNextNonSkipped` forward/backward/all-skipped/boundary.
3. **Integration test (alternate triage chord)** — The `D` chord is registered by default
   (`triage.contact_flip_tracked_advance`); same command machinery as item 1. A dedicated
   Catch2 test is optional; keymap tests cover the bridge.
4. **Manual — sub-25 FPS** — *Implementation done; smoke in fuzz.* Play at **0.5** on
   the preset ladder (~2 s per frame). Step **Rewind** / **FastForward** through
   the full `0.5 … 200` range while playing and paused; confirm `fps_label` and
   restored workspace JSON match. Automated: `TimeScrollBarMigratesLegacyPlaySpeedJson`
   (`tests/fuzz/unit/EditorState/fuzz_editor_state_from_json.cpp`).
5. **Manual — frame filter**: Mark frames as tracked, enable filter, verify
   arrow keys and playback skip tracked frames. Verify dynamic exclusion (mark →
   immediate skip).
6. **Manual — hotkey → command**: Bind a command sequence to a single key, press
   it, verify data mutation + frame advance. *(Automated smoke tests cover registration +
   `executeCommandSequenceFromRegistry`; full UI exercise remains manual.)*

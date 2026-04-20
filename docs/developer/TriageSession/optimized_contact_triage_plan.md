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

| Phase | Goal | Dependencies |
|-------|------|--------------|
| 1 | Atomic frame-level Commands | None |
| 2 | Hotkey → Command Sequence bridge | Phase 1 |
| 3 | Sub-25 FPS playback | None (parallel) |
| 4 | Dynamic frame filter on TimeScrollBar | None (parallel) |
| 5 | Adaptive FPS from ML confidence | Phases 3 & 4 |

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
| Timeline arrow keys (±1, ±N jump) | `TimeScrollBar` + `KeymapSystem` |
| Playback ≥ 25 FPS | `TimeScrollBar` — QTimer at 40 ms × integer `_play_speed` |

### What is Missing

| Capability | Notes |
|---|---|
| Sub-25 FPS playback | Timer period hardcoded at 40 ms; `_play_speed` is `int ≥ 1` |
| Frame filtering / skip during playback | No filter concept in `TimeScrollBar` or `TimeFrame` |
| Direct hotkey → command sequence binding | `KeymapSystem` and Commands are architecturally separate |
| Atomic commands for flip / mark-tracked / advance | Only widget-level methods exist; no `ICommand` implementations |
| Adaptive FPS from ML confidence | No infrastructure |

---

## Phase 1 — Atomic Frame-Level Commands

Create three new commands in `src/Commands/` that serve as composable
primitives for triage hotkey sequences. No dependencies on other phases.

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

- **Parameters:** `delta` (int, default 1), `time_key` (optional string)
- **Action:** Uses `TimeController` (added to `CommandContext`) to advance the current time position.
- **Category:** `ui_navigation`

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

Depends on Phase 1. Currently `KeymapSystem` and `Commands` are architecturally
separate; only `TriageSession` bridges them. This phase creates a direct path.

### 2.1 `registerCommandAction()` on `KeymapManager`

Add a new registration method:

```
registerCommandAction(action_id, display_name, category,
                      default_binding, CommandSequenceDescriptor)
```

On key press, if the resolved action is a command-action, `KeymapManager`:

1. Builds a `CommandContext` with `DataManager`, `TimeController`, and runtime
   variables (`${current_frame}` from `TimeController::currentPosition()`,
   `${current_time_key}` from the active TimeKey).
2. Calls `executeSequence()`.

`KeymapManager` obtains the `TimeController *` via
`EditorRegistry::timeController()`. The `CommandContext` itself remains Qt-free.

### 2.2 `TimeController` on `CommandContext`

Add an optional `TimeController *` field to `CommandContext` (see
[TimeController Extraction Roadmap](../ui/EditorState/time_controller_extraction_roadmap.md)).
`TimeController` is a Qt-free class depending only on `TimeFrame`, so this does
not introduce any Qt dependency into the Command architecture. This allows
`AdvanceFrame` (and future UI-navigation commands) to work as regular commands
inside sequences.

### 2.3 User-Configurable Hotkeys

The `KeybindingEditor` already shows all registered actions. Command-actions
appear alongside widget actions. The `TriageWidget`'s guided pipeline editor
can export sequences as hotkey-bound actions.

Example bindings:

| Key | Sequence |
|-----|----------|
| `F` | SetEventAtTime(contact, true) + SetEventAtTime(tracked, true) + AdvanceFrame(1) |
| `D` | FlipEventAtTime(contact) + SetEventAtTime(tracked, true) + AdvanceFrame(1) |
| `G` | SetEventAtTime(tracked, true) + AdvanceFrame(1) |
| `Right` | AdvanceFrame(1) *(existing — unchanged)* |

---

## Phase 3 — Sub-25 FPS Playback

Independent of Phases 1–2; can be developed in parallel.

### 3.1 Variable Timer Period

Currently `TimeScrollBar` uses a fixed 40 ms timer (25 Hz) and an integer
`_play_speed` multiplier (≥ 1), giving a minimum of 25 FPS.

Refactor to:

- Replace `int _play_speed` with `float _target_fps`.
- Timer period becomes `1000.0 / _target_fps` ms, always advancing 1 frame per
  tick.
- Supported range: 0.5–500+ FPS.

### 3.2 Discrete FPS Presets

Rewind / FastForward buttons step through discrete presets:

```
0.5  →  1  →  2  →  5  →  10  →  25  →  50  →  100  →  200
```

The `fps_label` displays the current preset. Persist `target_fps` as `float` in
`TimeScrollBarStateData`.

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
| `src/Commands/Core/CommandContext.hpp` | 2 | Add optional `TimeController *` field |
| `src/Commands/CMakeLists.txt` | 2 | Add `TimeController` dependency (Qt-free) |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` / `.cpp` | 2 | Add `registerCommandAction()` |
| `src/WhiskerToolbox/KeymapSystem/KeyAction.hpp` | 2 | Extend action descriptor for command sequences |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` / `.cpp` | 3, 4 | Playback refactor + filter integration |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBarStateData.hpp` | 3, 4 | Persist `target_fps` and filter settings |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBarRegistration.cpp` | 3 | Update FPS-related action registration if needed |
| `src/MLCore/models/supervised/HiddenMarkovModelOperation.hpp` / `.cpp` | 5 (Option B) | Expose $\xi$ computation |

## Files to Create

| File | Phase | Purpose |
|------|-------|---------|
| `src/Commands/SetEventAtTime/` | 1 | New command |
| `src/Commands/FlipEventAtTime/` | 1 | New command |
| `src/Commands/AdvanceFrame/` | 1 | New command |
| `src/WhiskerToolbox/TimeScrollBar/FrameFilter.hpp` / `.cpp` | 4 | Frame filter interface + concrete implementation |
| `src/WhiskerToolbox/TimeScrollBar/AdaptiveFPSController.hpp` / `.cpp` | 5 | FPS modulation controller |

---

## Verification

1. **Catch2 tests for new commands**: Exercise `SetEventAtTime` and
   `FlipEventAtTime` with a `DataManager` containing a
   `DigitalIntervalSeries`. Verify flip toggles correctly; verify
   `${current_frame}` variable substitution.
2. **Catch2 test for `FrameFilter`**: Test `shouldSkip()` with known intervals.
   Verify boundary behavior (first frame, last frame, empty series, fully
   tracked).
3. **Integration test for command sequences**: JSON sequence "flip contact +
   mark tracked + advance frame" via `executeSequence()`. Verify all three
   mutations occur.
4. **Manual — sub-25 FPS**: Play video at 0.5 FPS, verify frames advance
   approximately every 2 seconds. Test speed transitions across the full preset
   range.
5. **Manual — frame filter**: Mark frames as tracked, enable filter, verify
   arrow keys and playback skip tracked frames. Verify dynamic exclusion (mark →
   immediate skip).
6. **Manual — hotkey → command**: Bind a command sequence to a single key, press
   it, verify data mutation + frame advance.

# Data Synthesizer — Development Roadmap

## Progress

| Milestone | Status | Date |
|-----------|--------|------|
| **1** — Core Library, Generators & Command | ✅ Complete | 2026-03-12 |
| **2a** — Widget Skeleton & Registration | ✅ Complete | 2026-03-12 |
| **2b** — State & AutoParamWidget Integration | ✅ Complete | 2026-03-12 |
| **2c** — OpenGL Signal Preview | ✅ Complete | 2026-03-12 |
| **3** — More AnalogTimeSeries Generators | 🔲 Not started | — |
| **4** — DigitalEventSeries & DigitalIntervalSeries Generators | ✅ Complete | 2026-03-14 |
| **5a** — Static Shape Generators | ✅ Complete | — |
| **5b-i** — Trajectory Library + MovingPoint | ✅ Complete | 2026-03-14 |
| **5b-ia** — Enum & Variant Params for Motion Generators | ✅ Complete | 2026-03-14 |
| **5b-ii** — MovingMask + MovingLine Generators | 🔲 Not started | — |
| **5b-iii** — Time-Varying Shape (Area-Driven Mask) | 🔲 Not started | — |
| **6** — Multi-Signal Generation & Correlation | 🔲 Not started | — |
| **7** — Pipeline & Fuzz Testing Integration | 🔲 Not started | — |
| **8** — GUI Enhancements | 🔲 Not started | — |

---

## Motivation

WhiskerToolbox needs a built-in data synthesis system to:

1. **Testing**: Generate deterministic pseudo-random data with fixed seeds for reproducible unit tests, fuzz testing of transforms/commands, and round-trip validation without shipping large datasets.
2. **Documentation**: Tutorials and how-to guides can use synthesized data so users can follow along without needing the "right" dataset.
3. **Null hypothesis exploration**: Users can generate signals with known statistical properties (autocorrelation, spectral content, event rates) to compare against real neural data, avoiding the pitfall of testing against featureless white noise.

## Design Principles

- **Registry-based extensibility**: New generators (sine, square, Poisson, circle mask, …) register themselves statically, exactly like TransformsV2 transforms. Adding a new waveform should be a self-contained `.cpp` file with zero modifications to the registry or factory.
- **ParameterSchema-driven UI**: Every generator's params struct uses reflect-cpp. `AutoParamWidget` renders the form automatically. No hand-coded UI per generator.
- **Command Architecture integration**: A `SynthesizeData` command creates data in `DataManager` from a JSON descriptor. This means synthesis is composable inside pipelines, scriptable, and undo-able.
- **Deterministic by default**: Every stochastic generator requires a `seed` field. Same seed → same output, always.
- **Incremental delivery**: Each milestone is independently useful and testable.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    DataSynthesizer                       │
│  src/DataSynthesizer/                                   │
│                                                         │
│  ┌───────────────────┐   ┌────────────────────────────┐ │
│  │  GeneratorRegistry │   │  Generators/               │ │
│  │  (singleton)       │   │  ├─ SineWaveGenerator      │ │
│  │                    │◄──│  ├─ SquareWaveGenerator    │ │
│  │  register(name,    │   │  ├─ GaussianNoiseGenerator │ │
│  │    factory_fn,     │   │  ├─ PoissonEventGenerator  │ │
│  │    metadata)       │   │  ├─ CircleMaskGenerator   │ │
│  │                    │   │  └─ ...                    │ │
│  └────────┬──────────┘   └────────────────────────────┘ │
│           │                                             │
│  ┌────────▼──────────┐                                  │
│  │  SynthesizeData   │  (ICommand)                      │
│  │  command           │                                  │
│  └────────┬──────────┘                                  │
│           │                                             │
│           ▼                                             │
│     DataManager::setData<T>(key, data, time_key)        │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│              DataSynthesizer_Widget                      │
│  src/WhiskerToolbox/DataSynthesizer_Widget/             │
│                                                         │
│  ┌──────────────────┐  ┌──────────────────────────────┐ │
│  │ DataSynthesizer  │  │ Properties Widget            │ │
│  │ State            │  │ (output type/generator       │ │
│  │ (EditorState)    │  │  combos + AutoParamWidget)   │ │
│  └──────────────────┘  └──────────────────────────────┘ │
│                                                         │
│  ┌──────────────────────────────────────────────────────┐│
│  │ Preview Widget (QOpenGLWidget)                       ││
│  │  CorePlotting::SceneBuilder → PlottingOpenGL::       ││
│  │  SceneRenderer → PolyLineRenderer                   ││
│  └──────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
```

---

## Milestone 1 — Core Library, Generators & Command ✅ Complete (2026-03-12)

`DataSynthesizer` static library with `GeneratorRegistry` singleton, five `AnalogTimeSeries` generators (SineWave, SquareWave, TriangleWave, GaussianNoise, UniformNoise), and `SynthesizeData` command. Generators self-register at static init via `RegisterGenerator<Params>` (same `--whole-archive` pattern as TransformsV2). See `GeneratorRegistry.qmd` and `Analog.qmd` for details.

---

## Milestone 2 — GUI Widget

**Goal**: A dockable widget where users can select a generator, configure params, preview the output, and generate data into `DataManager`.

### 2a. Widget Skeleton & Registration ✅ Complete (2026-03-12)

Delivered `DataSynthesizerState`/`DataSynthesizerStateData` (`EditorState` subclass with `instance_id`/`display_name`), empty properties and view `QWidget` stubs, and `DataSynthesizerWidgetRegistration` registering under `"View/Tools"`. Static library `DataSynthesizer_Widget` depends on `Qt6::Widgets`, `EditorState`, `DataManager`.

---

### 2b. State & AutoParamWidget Integration ✅ Complete (2026-03-12)

`DataSynthesizerStateData` expanded with `output_type`, `generator_name`, `parameter_json`, `output_key`, and `time_key`. Properties widget wired with output type combo → generator combo → `AutoParamWidget` → output key field → Generate button. `GeneratorRegistry::listOutputTypes()` added. State restore uses `_restoring` guard to prevent recursive updates. CMake links `AutoParamWidget`, `DataSynthesizer` (with `--whole-archive`), `ParameterSchema`, `Commands`.

---

### 2c. OpenGL Signal Preview ✅ Complete (2026-03-12)

`SynthesizerPreviewWidget` (`QOpenGLWidget`) added using `PlottingOpenGL::SceneRenderer` + `TimeSeriesMapper::mapAnalogSeriesFull()` + `glm::ortho()` projection. Non-interactive; renders a single static polyline. View widget hosts the preview widget. Properties widget gained a "Preview" button that calls `GeneratorRegistry::generate()` directly (ephemeral — no DataManager involvement) and emits `previewRequested(series)`. Registration wires `previewRequested` → `setPreviewData`. CMake adds `PlottingOpenGL`, `CorePlotting`, `Qt6::OpenGL`, `Qt6::OpenGLWidgets`, `glm::glm`.

---

### Milestone 3 — More AnalogTimeSeries Generators (Noise, Composites)

**Goal**: Expand the analog generator catalog to cover common scientific use cases.

| Generator | Params | Notes |
|-----------|--------|-------|
| **SawtoothWave** | `{num_samples, amplitude, frequency, phase, dc_offset}` | Rising ramp with reset. |
| **ColoredNoise** | `{num_samples, exponent, amplitude, seed}` | Exponent: 0=white, 1=pink, 2=brown/red. Spectral shaping via FFT. |
| **BandLimitedNoise** | `{num_samples, low_freq, high_freq, amplitude, seed}` | FFT-based bandpass. |
| **SinePlusNoise** | `{num_samples, amplitude, frequency, noise_stddev, snr_db, seed}` | Composite: user specifies either `noise_stddev` or `snr_db`. |
| **Chirp** | `{num_samples, start_freq, end_freq, amplitude}` | Linear frequency sweep. |
| **AutocorrelatedNoise** | `{num_samples, decay_constant, amplitude, seed}` | AR(1) process with specified lag-1 autocorrelation. |
| **StereotypeEventsPlusNoise** | `{num_samples, event_template, event_times, snr_db, seed}` | Place a known waveform template at specified times, add Gaussian noise. |

Each generator is a self-contained `.cpp`, self-registering, with unit tests.

**Milestone 3 exit criteria**: All generators register, produce correct output sizes, are deterministic with seed, and appear in the GUI combo box automatically.

---

### Milestone 4 — DigitalEventSeries & DigitalIntervalSeries Generators

**Goal**: Extend synthesis to event/interval data types.

#### Completed (2026-03-14)

| Generator | Output Type | Params | Notes |
|-----------|------------|--------|-------|
| **PoissonEvents** | `DigitalEventSeries` | `{num_samples, lambda, seed}` | Homogeneous Poisson process. |
| **RegularEvents** | `DigitalEventSeries` | `{num_samples, interval, jitter_stddev, seed}` | Evenly spaced with optional Gaussian jitter. |
| **BurstEvents** | `DigitalEventSeries` | `{num_samples, burst_rate, within_burst_rate, burst_duration, seed}` | Clustered event patterns. |
| **InhomogeneousPoissonEvents** | `DigitalEventSeries` | `{rate_signal_key, rate_scale, seed}` | Inhomogeneous Poisson process. Reads λ(t) from an AnalogTimeSeries in DataManager. Uses Lewis-Shedler thinning algorithm. First contextual generator (requires `GeneratorContext`). |
| **RegularIntervals** | `DigitalIntervalSeries` | `{num_samples, on_duration, off_duration, start_offset}` | Periodic on/off. |
| **RandomIntervals** | `DigitalIntervalSeries` | `{num_samples, mean_duration, mean_gap, seed}` | Exponentially distributed durations and gaps. |

The `SynthesizeData` command was updated to extract sample counts from `DigitalEventSeries` and `DigitalIntervalSeries` via `->size()`. CMake links `WhiskerToolbox::DigitalTimeSeries`.

The `GeneratorContext` infrastructure was added to support generators that need DataManager access. `GeneratorFunction` signature now includes `GeneratorContext const &`, and `RegisterGenerator` provides overloads for both context-aware and context-free generators. Existing generators are unaffected.

#### Future Extension — Nested Generator Specs

The current `InhomogeneousPoissonEvents` implementation takes a `rate_signal_key` to read an existing AnalogTimeSeries from the DataManager. A future extension could allow specifying the rate signal as a nested generator descriptor (e.g., `{generator_name: "SineWave", parameters: {...}}`), which would generate the rate signal on-the-fly. This pattern generalizes to any generator that needs a time-varying input.

---

### Milestone 5 — Spatial Data Generators (Mask, Point, Line)

**Goal**: Generate spatiotemporal data (masks, points, lines) with motion models.

#### 5a. Static Shape Generators ✅

All six static shape generators are implemented and tested.
See [Spatial Generators](Generators/Spatial.qmd) for full documentation.

| Generator | Output Type | Params |
|-----------|------------|--------|
| **CircleMask** | `MaskData` | `{image_width, image_height, center_x, center_y, radius, num_frames}` |
| **RectangleMask** | `MaskData` | `{image_width, image_height, center_x, center_y, width, height, num_frames}` |
| **EllipseMask** | `MaskData` | `{image_width, image_height, center_x, center_y, semi_major, semi_minor, angle, num_frames}` |
| **GridPoints** | `PointData` | `{rows, cols, spacing, origin_x, origin_y, num_frames}` |
| **RandomPoints** | `PointData` | `{num_points, min_x, max_x, min_y, max_y, num_frames, seed}` |
| **StraightLine** | `LineData` | `{start_x, start_y, end_x, end_y, num_points_per_line, num_frames}` |

#### 5b. Motion Models (Phase 2 of spatial)

Motion models add time-varying position to static shape generators. The design separates
three concerns — *where* something is, *what* it looks like there, and *how* it gets stored
— so that motion logic is shared across all spatial data types without a proliferation of
generator types.

##### Design: Trajectory → Shape → Populate Pipeline

Every moving spatial generator follows the same three-stage pipeline:

```
  1. Trajectory          2. Shape                    3. Populate
  (physics)              (rendering)                 (data storage)

  computeTrajectory()    renderShape(pos, t, ...)    addAtTime(t, shape)
  ─────────────────►     ─────────────────────►      ──────────────────►
  produces               produces per-frame          writes into
  vector<Point2D<float>> Mask2D / Line2D /           MaskData / LineData /
  (one position per      Point2D<float>              PointData
  frame)                 at that position
```

1. **Trajectory** — pure physics. A shared `computeTrajectory()` function takes a starting
   position, frame count, and motion parameters and returns a sequence of 2D positions.
   Motion model (linear / sinusoidal / brownian) and boundary mode (clamp / bounce / wrap)
   are handled here. This code is the same regardless of what is being moved.

2. **Shape** — what the object looks like at a given position and time. In the simplest
   case, the shape is *static*: the same circle / line / point cloud every frame, just
   repositioned. In a more advanced case, the shape can *vary with time* (e.g., a mask
   whose radius is driven by an external `AnalogTimeSeries`). The shape step also handles
   output-type-specific concerns like pixel clipping for masks.

3. **Populate** — writes the rendered shape into the output data object (`MaskData`,
   `LineData`, `PointData`) at each `TimeFrameIndex`. This is straightforward: one
   `addAtTime()` call per frame.

Because the trajectory is decoupled from the shape, we only need **one generator per output
type** rather than one per shape×motion combination. The shape type (circle vs rectangle vs
ellipse, single point vs grid, etc.) is a parameter inside the generator.

```
┌──────────────────┐
│  Trajectory      │     shared library, used by all generators
│  computeTrajectory()
└────────┬─────────┘
         │
    ┌────┴────────────────────┬──────────────────────┐
    ▼                        ▼                      ▼
┌────────────┐       ┌──────────────┐       ┌──────────────┐
│ MovingMask │       │ MovingLine   │       │ MovingPoint  │
│ Generator  │       │ Generator    │       │ Generator    │
│            │       │              │       │              │
│ shape=     │       │ translates   │       │ single point │
│ circle|    │       │ vertices     │       │ or rigid     │
│ rect|      │       │ rigidly      │       │ collection   │
│ ellipse    │       │              │       │              │
│            │       │ no clipping  │       │ no clipping  │
│ clips to   │       │              │       │              │
│ image      │       │              │       │              │
└────────────┘       └──────────────┘       └──────────────┘
  → MaskData           → LineData             → PointData
```

##### Trajectory Library API

`src/DataSynthesizer/Trajectory/Trajectory.hpp`

```cpp
struct TrajectoryParams {
    std::string model = "linear";           // "linear", "sinusoidal", "brownian"

    // Linear: position(t) = start + velocity * t
    float velocity_x = 1.0f;
    float velocity_y = 0.0f;

    // Sinusoidal: position(t) = start + amplitude * sin(2π * frequency * t + phase)
    float amplitude_x = 0.0f;
    float amplitude_y = 0.0f;
    float frequency_x = 0.0f;
    float frequency_y = 0.0f;
    float phase_x = 0.0f;
    float phase_y = 0.0f;

    // Brownian: position(t) = position(t-1) + N(0, diffusion)
    float diffusion = 1.0f;
    uint64_t seed = 42;

    // Boundary behavior
    std::string boundary_mode = "clamp";    // "clamp", "bounce", "wrap"
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

/// Compute a trajectory of num_frames positions starting from (start_x, start_y).
/// Boundary enforcement is applied at each step.
std::vector<Point2D<float>> computeTrajectory(
    float start_x, float start_y,
    int num_frames,
    TrajectoryParams const & params);
```

Boundary enforcement is applied per-step (important for Brownian motion where per-step
clamping produces different results than post-hoc clamping). The trajectory operates on
a **reference point** — the point itself, the mask center, the line centroid, or the
point-cloud origin. What that reference point *means* is the generator's concern.

##### Shape Rendering by Output Type

Each output type handles the trajectory→shape step differently:

| Output type | Reference point | Shape step | Boundary interaction |
|-------------|----------------|------------|---------------------|
| **PointData** (single) | The point itself | Identity — position *is* the shape | None |
| **PointData** (collection) | Collection origin | Rigid-body translate all points | None — individual points may leave bounds |
| **MaskData** | Shape center | Re-rasterize shape at position (e.g., `generate_ellipse_pixels`) | **Clip** pixels to `[0, image_width) × [0, image_height)` — partial occlusion is correct |
| **LineData** | Line centroid | Rigid-body translate all vertices | None — vertices are float-valued, renderer handles visibility |

For masks, a shared `clipPixelsToImage()` utility filters out-of-bounds pixels after
rasterization. A shape near the edge will have fewer pixels — this is the desired behavior
(the shape is partially off-screen, so its area decreases near boundaries).

##### On Multi-Entity

`RaggedTimeSeries` already stores multiple entries per frame, so the data model supports
multiple objects naturally. From a trajectory perspective, multi-entity is just "compute
N trajectories and render N shapes per frame". The generator can accept a count and
spread/arrangement parameters to generate starting positions, then run `computeTrajectory`
once per entity. No special `MultiMovingCircleMaskParams` type needed — the existing
`MovingMask` generator gains an optional `count` field, and starting positions are derived
from a simple layout (e.g., evenly spaced, random). Per-entity motion overrides can be
deferred until there is a real need.

---

##### 5b-i. Trajectory Library + MovingPoint Generator ✅ Complete (2026-03-14)

**Goal**: Build the reusable trajectory library and validate it with the simplest generator.

**Deliverables:**

1. **Trajectory library** (`src/DataSynthesizer/Trajectory/Trajectory.hpp` + `.cpp`):
   - `TrajectoryParams` struct (reflect-cpp enabled).
   - `computeTrajectory()` free function.
   - Three motion models: **linear**, **sinusoidal**, **brownian** (deterministic with seed).
   - Three boundary modes: **clamp**, **bounce**, **wrap**.
   - `clipPixelsToImage()` utility for mask generators (header-only, used later in 5b-ii).

2. **`MovingPoint` generator** (`src/DataSynthesizer/Generators/Point/MovingPointGenerator.cpp`):
   - Produces a `PointData` with exactly one point per frame.
   - Parameters: `start_x`, `start_y`, `num_frames`, plus flattened `TrajectoryParams` fields.
   - Calls `computeTrajectory()`, then `addAtTime()` for each frame.

3. **Tests**:
   - `Trajectory.test.cpp`: unit tests for `computeTrajectory()` — trajectory length, each
     motion model produces expected positions, each boundary mode behaves correctly, determinism.
   - `MovingPointGenerator.test.cpp`: linear motion reaches expected final position, sinusoidal
     returns near start after one period, clamp/bounce/wrap at boundary, zero velocity is
     stationary, single frame is start position.

**Exit criteria**: Trajectory library has independent unit tests. `MovingPoint` generator
registered and produces correct trajectories for all motion × boundary combinations.

**Implementation note**: Generator params structs with many fields must use `std::optional`
for each trajectory-related field with `.value_or()` defaults. reflect-cpp does not reliably
support aggregate default values when many fields are missing from JSON. This matches the
established TransformsV2 pattern (e.g., `LineAngleParams`). The `TrajectoryParams` struct
itself uses plain defaults since it is never parsed from JSON directly — generators convert
their optional fields to `TrajectoryParams` via a `toTrajectoryParams()` helper.

---

##### 5b-ia. Enum & Variant Params for Motion Generators ✅ Complete (2026-03-14)

**Goal**: Refactor `MovingPointParams` (and the future `MovingMask`/`MovingLine` params) to
use native `enum class` for discriminator fields and `rfl::TaggedUnion` for mutually-exclusive
motion parameters. This eliminates the confusing flat-optional UI and replaces it with a
combo-box-driven sub-form that shows only the relevant fields.

**Depends on**: ParameterSchema enum support (Phase 1) and variant support (Phase 2) from the
[ParameterSchema Enum & Variant Roadmap](../ParameterSchema/enum_variant_roadmap.qmd).

**Problem**: `MovingPointParams` currently has 18 optional fields for three mutually-exclusive
motion models (linear, sinusoidal, brownian) plus boundary parameters. The `model` and
`boundary_mode` fields are `std::optional<std::string>`, rendered as free-text `QLineEdit`
widgets. Users see all 18 fields simultaneously with no indication that most are irrelevant
for their chosen model.

**Solution**: Restructure the params into typed sub-structs and use `rfl::TaggedUnion` to
represent the motion model choice.

###### Before (current)

```cpp
struct MovingPointParams {
    std::optional<float> start_x;
    std::optional<float> start_y;
    int num_frames = 100;
    std::optional<std::string> model;         // "linear", "sinusoidal", "brownian"
    std::optional<float> velocity_x;          // linear only
    std::optional<float> velocity_y;          // linear only
    std::optional<float> amplitude_x;         // sinusoidal only
    std::optional<float> amplitude_y;         // sinusoidal only
    std::optional<float> frequency_x;         // sinusoidal only
    std::optional<float> frequency_y;         // sinusoidal only
    std::optional<float> phase_x;             // sinusoidal only
    std::optional<float> phase_y;             // sinusoidal only
    std::optional<float> diffusion;           // brownian only
    std::optional<uint64_t> seed;             // brownian only
    std::optional<std::string> boundary_mode; // all
    // ... 4 more boundary fields ...
};
```

###### After (proposed)

```cpp
// Shared boundary params (used by all motion models and all moving generators)
enum class BoundaryMode { clamp, bounce, wrap };

struct BoundaryParams {
    BoundaryMode boundary_mode = BoundaryMode::clamp;
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

// Per-model params
struct LinearMotionParams {
    float velocity_x = 1.0f;
    float velocity_y = 0.0f;
};

struct SinusoidalMotionParams {
    float amplitude_x = 0.0f;
    float amplitude_y = 0.0f;
    float frequency_x = 0.0f;
    float frequency_y = 0.0f;
    float phase_x = 0.0f;
    float phase_y = 0.0f;
};

struct BrownianMotionParams {
    float diffusion = 1.0f;
    uint64_t seed = 42;
};

using MotionModelVariant = rfl::TaggedUnion<
    "model",
    LinearMotionParams,
    SinusoidalMotionParams,
    BrownianMotionParams>;

struct MovingPointParams {
    float start_x = 100.0f;
    float start_y = 100.0f;
    int num_frames = 100;
    MotionModelVariant motion;    // ← combo box + swapping sub-form
    BoundaryParams boundary;      // ← always visible, grouped
};
```

###### GUI Behavior

With ParameterSchema Phase 1 + Phase 2:

1. `start_x`, `start_y`, `num_frames` — always visible, non-optional.
2. `motion` — rendered as combo box (`"LinearMotionParams"` / `"SinusoidalMotionParams"` /
   `"BrownianMotionParams"`) with a `QStackedWidget` sub-form underneath. Selecting
   "Linear" shows `velocity_x` and `velocity_y`. Selecting "Sinusoidal" shows amplitude,
   frequency, and phase fields. Selecting "Brownian" shows `diffusion` and `seed`.
3. `boundary` — rendered as a collapsible group (using `ParameterUIHints::is_advanced`
   or a `group` annotation). Always visible regardless of motion model selection.

###### JSON Compatibility

The new JSON format is:
```json
{
    "start_x": 0,
    "start_y": 100,
    "num_frames": 200,
    "motion": {
        "model": "LinearMotionParams",
        "velocity_x": 2.0,
        "velocity_y": 0.5
    },
    "boundary": {
        "boundary_mode": "clamp",
        "bounds_min_x": 0,
        "bounds_max_x": 1000
    }
}
```

This is a **breaking change** from the flat-optional format. Since MovingPoint is new
(Milestone 5b-i was just completed), there are no external consumers to migrate.

###### Shared Sub-Structs for MovingMask and MovingLine

`MotionModelVariant` and `BoundaryParams` are defined once (in the Trajectory library
or a shared header) and reused by all moving generators. This means MovingMask (5b-ii)
and MovingLine (5b-ii) automatically get the same combo-box UI with no per-generator
annotation.

###### `toTrajectoryParams()` Update

The `toTrajectoryParams()` helper is updated to construct `TrajectoryParams` from the
typed variant using `std::visit` (or `rfl::visit`):

```cpp
TrajectoryParams toTrajectoryParams(
    MotionModelVariant const & motion,
    BoundaryParams const & boundary) {
    return motion.variant().visit([&](auto const & model_params) {
        TrajectoryParams tp;
        tp.boundary_mode = to_string(boundary.boundary_mode);
        tp.bounds_min_x = boundary.bounds_min_x;
        // ... other boundary fields ...
        if constexpr (std::is_same_v<decltype(model_params), LinearMotionParams const &>) {
            tp.model = "linear";
            tp.velocity_x = model_params.velocity_x;
            tp.velocity_y = model_params.velocity_y;
        } else if constexpr (...) {
            // sinusoidal, brownian branches
        }
        return tp;
    });
}
```

`TrajectoryParams` itself remains a plain internal struct with string-based model/mode
fields — it is never exposed to JSON or the UI.

###### Deliverables

| Item | Description |
|------|-------------|
| `BoundaryMode` enum | Shared `enum class` for boundary modes |
| `BoundaryParams` struct | Shared boundary fields (non-optional, with defaults) |
| `LinearMotionParams` / `SinusoidalMotionParams` / `BrownianMotionParams` | Per-model param sub-structs |
| `MotionModelVariant` | `rfl::TaggedUnion<"model", ...>` type alias |
| Refactored `MovingPointParams` | Uses `MotionModelVariant` + `BoundaryParams` |
| Updated `toTrajectoryParams()` | Dispatches via variant visitor |
| Updated tests | `MovingPointGenerator.test.cpp` adapted to new JSON format |
| Updated `MotionModels.qmd` | Developer docs reflect new param structure |

###### Exit Criteria

- `MovingPoint` generator produces identical trajectories as before (same math, different params API).
- All existing `MovingPointGenerator.test.cpp` tests pass with updated JSON.
- `Trajectory.test.cpp` tests unchanged (internal `TrajectoryParams` not modified).
- DataSynthesizer widget: selecting `MovingPoint` → `model` is a combo box → selecting
  a model shows only relevant fields → generating works end-to-end.

---

##### 5b-ii. MovingMask + MovingLine Generators 🔲

**Goal**: Apply the trajectory library to mask and line output types.

**Deliverables:**

1. **`MovingMask` generator** (`src/DataSynthesizer/Generators/Mask/MovingMaskGenerator.cpp`):
   - One generator, shape is a parameter: `shape = "circle" | "rectangle" | "ellipse"`.
   - Shape-specific fields: `radius` (circle), `width`/`height` (rectangle),
     `semi_major`/`semi_minor`/`angle` (ellipse). Unused fields are ignored.
   - `image_width`, `image_height`, `start_x`, `start_y`, `num_frames`, plus
     `MotionModelVariant` and `BoundaryParams` from 5b-ia.
   - At each frame: rasterize shape at trajectory position, clip to image bounds, store.
   - Uses `clipPixelsToImage()` from the trajectory library.

2. **`MovingLine` generator** (`src/DataSynthesizer/Generators/Line/MovingLineGenerator.cpp`):
   - Parameters: `start_x`, `start_y`, `end_x`, `end_y`, `num_points_per_line`, `num_frames`,
     plus `MotionModelVariant` and `BoundaryParams` from 5b-ia.
   - Line shape defined once as offsets from centroid. At each frame, all vertices translated
     by trajectory offset. No clipping.

3. **Tests**:
   - `MovingMaskGenerator.test.cpp`: each shape type moves correctly, pixel count preserved
     when interior, decreases near edges (clipping), area matches static generator when
     fully interior, deterministic Brownian.
   - `MovingLineGenerator.test.cpp`: centroid follows trajectory, relative vertex distances
     constant across frames, vertices may extend beyond bounds.

**Exit criteria**: Both generators registered. Mask clipping works at all edges. Line
translates rigidly. Tests pass.

---

##### 5b-iii. Time-Varying Shape — Area-Driven Mask 🔲

**Goal**: Demonstrate the "shape varies with time" capability of the pipeline.

In 5b-i and 5b-ii, the shape is static (same radius/dimensions every frame, just
repositioned). This phase adds a shape that changes over time: a mask whose area is
driven by an external `AnalogTimeSeries`.

This naturally composes with motion: the trajectory says *where* the shape is, and the
area signal says *how big* it is at each frame. Both are independent inputs to the
shape→populate step.

**Deliverables:**

1. **`AreaDrivenMask` generator** — either a new generator or an extension of `MovingMask`
   with an optional `area_signal_key` field:
   - When `area_signal_key` is set, reads an `AnalogTimeSeries` from `DataManager` via
     `GeneratorContext`. At each frame, derives the shape size from the area value
     (e.g., `radius = sqrt(area / π)` for circles).
   - When `area_signal_key` is not set, falls back to static shape parameters.
   - Composes with trajectory: the center moves per `TrajectoryParams`, the size varies
     per the area signal.

2. **Tests**:
   - Constant area signal produces same result as static `MovingMask`.
   - Varying area signal produces masks with correct pixel counts at each frame.
   - Round-trip validation: generate area signal → `AreaDrivenMask` → `CalculateMaskArea`
     transform → compare to original signal.

**Exit criteria**: Area-driven mask produces correct per-frame areas. Round-trip with
`CalculateMaskArea` validates within tolerance.

---

##### 5b Summary

| Sub-phase | Generator(s) | Output Type | Key Concept |
|-----------|-------------|-------------|-------------|
| **5b-i** | `MovingPoint` | `PointData` | Trajectory library (shared) |
| **5b-ia** | `MovingPoint` (refactor) | `PointData` | Enum & variant params (ParameterSchema Phase 1+2) |
| **5b-ii** | `MovingMask`, `MovingLine` | `MaskData`, `LineData` | Shape rendering + pixel clipping |
| **5b-iii** | `AreaDrivenMask` | `MaskData` | Time-varying shape |

All sub-phases share the same trajectory library from 5b-i. 5b-ia introduces shared
`MotionModelVariant` and `BoundaryParams` types that are reused by 5b-ii generators,
so those generators get combo-box-driven sub-forms automatically.

---

**Milestone 5 exit criteria**: Static shapes generate correctly (5a). Trajectory library
computes correct trajectories for linear, sinusoidal, and brownian models with clamp/bounce/wrap
boundary modes (5b-i). MovingMask, MovingLine, and MovingPoint generators produce time-varying
spatial data with correct boundary/clipping semantics (5b-i, 5b-ii). Area-driven mask validates
round-trip with `CalculateMaskArea` transform (5b-iii).

---

### Milestone 6 — Multi-Signal Generation & Correlation

**Goal**: Generate multiple correlated signals in a single command invocation.

#### 6a. `SynthesizeMultipleData` Command

```cpp
struct SignalSpec {
    std::string output_key;
    std::string generator_name;
    std::string output_type;
    rfl::Generic parameters;
};

struct SynthesizeMultipleDataParams {
    std::vector<SignalSpec> signals;
    std::optional<std::vector<std::vector<float>>> correlation_matrix;
    uint64_t seed = 42;
    std::string time_key = "time";
};
```

When `correlation_matrix` is provided, the command:
1. Generates all signals independently using their respective generators.
2. Applies Cholesky decomposition to impose the desired correlation structure on the noise/random components.
3. Stores all signals in `DataManager`.

This requires that generators expose a way to separate their "deterministic" and "stochastic" components, or that the correlation is applied as a post-processing step on the raw outputs.

#### 6b. Simpler Paired Generators

For common cases that don't need a full correlation matrix:

| Generator | Description |
|-----------|-------------|
| **CorrelatedGaussianPair** | Two Gaussian noise signals with specified Pearson correlation |
| **SignalPlusNoise** | One "clean" signal + one noisy copy with specified SNR |

**Milestone 6 exit criteria**: `SynthesizeMultipleData` command works. Two correlated Gaussian signals have measured correlation within tolerance of the specified value.

---

### Milestone 7 — Pipeline & Fuzz Testing Integration

**Goal**: Use the synthesizer + command architecture for automated testing.

#### 7a. Fuzz Test Harness

A test utility that:
1. Generates random `SynthesizeData` command JSON (random generator, random valid params, fixed seed).
2. Generates random transform/command pipeline JSON.
3. Executes: synthesize → pipeline → save → load → compare.
4. Verifies bit-exact reproduction.

This lives in `tests/fuzz/` and uses the existing fuzz test infrastructure.

#### 7b. Golden-Value Regression Tests

For each generator, a test that:
1. Runs the generator with a known seed and known params.
2. Compares output against a checked-in golden values file (e.g., first 10 values + stats).
3. Fails if the output changes, catching accidental changes to generator algorithms.

#### 7c. Documentation Example Pipelines

JSON pipeline files in `examples/` that demonstrate:
- Synthesize a sine wave → apply a transform → save the result.
- Synthesize Poisson events → convert to interval → save.
- Synthesize a mask time series → calculate area → compare to original.

These serve as both documentation and integration tests.

**Milestone 7 exit criteria**: Fuzz tests pass. Golden-value tests pinned. Example pipelines execute without error.

---

### Milestone 8 — GUI Enhancements

**Goal**: Polish the GUI for power-user workflows.

| Feature | Description |
|---------|-------------|
| **Waveform superposition UI** | "Add another waveform" button that enables stacking generators (sine + noise). Internally generates a `SinePlusNoise` or composite descriptor. |
| **Parameter presets** | Save/load named parameter sets (e.g., "60 Hz line noise", "Poisson λ=30 Hz"). Stored as JSON in a user-config directory. |
| **Batch generation** | Generate N signals at once with systematic parameter variation (e.g., sweep frequency from 1–100 Hz in 10 steps). |
| **Live preview update** | Auto-regenerate preview on parameter change (debounced). |
| **Drag-to-DataManager** | Drag the preview into the DataManager_Widget data tree to name and place it. |

---

### Future Directions (Not Scheduled)

These are ideas noted for future consideration, not committed milestones:

- **Modulation**: AM/FM modulation of any periodic generator by another generator.
- **Convolution / filtering**: Apply FIR/IIR filters to generated signals.
- **Import external specifications**: Load generator configs from external tool formats (e.g., MATLAB struct descriptions).
- **Statistical summary widget**: Show autocorrelation, PSD, histogram of generated signal alongside a real signal for comparison.
- **TensorData generators**: Random matrices, identity, structured tensors for ML pipeline testing.
- **Multi-entity count parameter**: Add optional `count` + layout params to moving generators for multiple independently moving objects in the same data object.
- **Replay / animation**: Step through time frames in the preview to see spatial data evolving.

---

## Current File Layout

```
src/
├── DataSynthesizer/                          # ✅ Milestones 1 & 2 (complete)
│   ├── CMakeLists.txt
│   ├── GeneratorRegistry.hpp
│   ├── GeneratorRegistry.cpp
│   ├── GeneratorTypes.hpp
│   ├── Registration.hpp
│  ├─ Trajectory/                            # ✅ Milestone 5b-i (complete)
│  │  ├─ Trajectory.hpp
│  │  ├─ Trajectory.cpp
│  │  ├─ MotionParams.hpp                     # ✅ Milestone 5b-ia (complete)
│  │  └─ PixelClipping.hpp
│  ├─ Generators/
│  │  ├─ Analog/
│  │  │  ├─ SineWaveGenerator.cpp
│  │  │  ├─ SquareWaveGenerator.cpp
│  │  │  ├─ TriangleWaveGenerator.cpp
│  │  │  ├─ GaussianNoiseGenerator.cpp
│  │  │  └─ UniformNoiseGenerator.cpp
│  │  ├─ DigitalEvent/
│  │  │  ├─ PoissonEventsGenerator.cpp
│  │  │  ├─ RegularEventsGenerator.cpp
│  │  │  ├─ BurstEventsGenerator.cpp
│  │  │  └─ InhomogeneousPoissonEventsGenerator.cpp
│  │  ├─ DigitalInterval/
│  │  │  ├─ RegularIntervalsGenerator.cpp
│  │  │  └─ RandomIntervalsGenerator.cpp
│  │  ├─ Mask/
│  │  │  ├─ CircleMaskGenerator.cpp          # ✅ 5a
│  │  │  ├─ RectangleMaskGenerator.cpp       # ✅ 5a
│  │  │  ├─ EllipseMaskGenerator.cpp         # ✅ 5a
│  │  │  └─ MovingMaskGenerator.cpp          # 🔲 5b-ii
│  │  ├─ Point/
│  │  │  ├─ GridPointsGenerator.cpp          # ✅ 5a
│  │  │  ├─ RandomPointsGenerator.cpp        # ✅ 5a
│  │  │  └─ MovingPointGenerator.cpp         # ✅ 5b-i
│  │  └─ Line/
│  │     ├─ StraightLineGenerator.cpp        # ✅ 5a
│  │     └─ MovingLineGenerator.cpp          # 🔲 5b-ii
├── Commands/                                 # ✅ Milestone 1 (complete)
│   ├── SynthesizeData.hpp
│   └── SynthesizeData.cpp
└── WhiskerToolbox/
    └── DataSynthesizer_Widget/               # ✅ Milestone 2 (complete)
        ├── CMakeLists.txt
        ├── DataSynthesizerWidgetRegistration.hpp
        ├── DataSynthesizerWidgetRegistration.cpp
        ├── Core/
        │   ├── DataSynthesizerStateData.hpp
        │   ├── DataSynthesizerState.hpp
        │   └── DataSynthesizerState.cpp
        └── UI/
            ├── DataSynthesizerProperties_Widget.hpp
            ├── DataSynthesizerProperties_Widget.cpp
            ├── DataSynthesizerView_Widget.hpp
            ├── DataSynthesizerView_Widget.cpp
            ├── SynthesizerPreviewWidget.hpp
            └── SynthesizerPreviewWidget.cpp

tests/
└── DataSynthesizer/                          # ✅ Milestone 1 (complete)
    ├── CMakeLists.txt
    ├── GeneratorRegistry.test.cpp
    ├── SineWaveGenerator.test.cpp
    ├── SquareWaveGenerator.test.cpp
    ├── TriangleWaveGenerator.test.cpp
    ├── GaussianNoiseGenerator.test.cpp
    ├── UniformNoiseGenerator.test.cpp
    ├── SynthesizeDataCommand.test.cpp
    ├── PoissonEventsGenerator.test.cpp
    ├── RegularEventsGenerator.test.cpp
    ├── BurstEventsGenerator.test.cpp
    ├── InhomogeneousPoissonEventsGenerator.test.cpp
    ├── RegularIntervalsGenerator.test.cpp
    ├── RandomIntervalsGenerator.test.cpp
    ├── Trajectory.test.cpp                   # ✅ 5b-i (complete)
    ├── MovingPointGenerator.test.cpp         # ✅ 5b-i (complete)
    ├── MovingMaskGenerator.test.cpp          # 🔲 5b-ii (planned)
    └── MovingLineGenerator.test.cpp          # 🔲 5b-ii (planned)

docs/
└── developer/
    └── DataSynthesizer/
        ├── ROADMAP.md              (this file)
        ├── GeneratorRegistry.qmd   # ✅ Milestone 1
        ├── Widget.qmd              # ✅ Milestone 2
        └── Generators/
            ├── Analog.qmd          # ✅ Milestone 1
            ├── DigitalEvent.qmd    # ✅ Milestone 4
            ├── DigitalInterval.qmd # ✅ Milestone 4
            ├── Spatial.qmd         # ✅ Milestone 5a
            └── MotionModels.qmd    # ✅ Milestone 5b-i
```

## Key Design Decisions

### Why a separate `DataSynthesizer` library (not TransformsV2)?

TransformsV2 transforms take input data and produce output data. Generators take *no input data* — they create data from parameters alone. While the registration mechanics are similar, the conceptual model is different enough to warrant a separate registry. This avoids awkward "null input" transforms and keeps the TransformsV2 type system clean.

### Why `rfl::Generic` for generator parameters in the command?

The `SynthesizeData` command doesn't know which generator will be used at compile time. The generator-specific params are deserialized inside the generator's factory function, using the same `rfl::json::read<SpecificParams>(params_json)` pattern as `CommandFactory`. This keeps the command interface stable while generators define their own parameter types.

### Why `--whole-archive`?

Static registration relies on side effects of constructing file-scope objects. If the linker sees no symbol references into a translation unit, it may discard it entirely, and the registration never happens. `--whole-archive` forces all object files in the static library to be linked. This is a known requirement also used by TransformsV2 tests.

### Why not put generators directly in the Command?

Separation of concerns. The `GeneratorRegistry` is usable from tests, benchmarks, and other non-command contexts. The command is just one consumer of the registry. For example, the GUI preview uses the registry directly without going through the command.

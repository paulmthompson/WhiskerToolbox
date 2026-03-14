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
| **5** — Spatial Data Generators | 🔲 Not started | — |
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

#### 5a. Static Shape Generators

| Generator | Output Type | Params |
|-----------|------------|--------|
| **CircleMask** | `MaskData` | `{image_width, image_height, center_x, center_y, radius, num_frames}` |
| **RectangleMask** | `MaskData` | `{image_width, image_height, center_x, center_y, width, height, num_frames}` |
| **EllipseMask** | `MaskData` | `{image_width, image_height, center_x, center_y, semi_major, semi_minor, angle, num_frames}` |
| **GridPoints** | `PointData` | `{rows, cols, spacing, origin_x, origin_y, num_frames}` |
| **RandomPoints** | `PointData` | `{num_points, min_x, max_x, min_y, max_y, num_frames, seed}` |
| **StraightLine** | `LineData` | `{start_x, start_y, end_x, end_y, num_points_per_line, num_frames}` |

#### 5b. Motion Models (Phase 2 of spatial)

A `MotionModel` is a composable modifier applied on top of a static shape generator to make it vary over time:

```cpp
struct MotionModelParams {
    std::string model;            // "linear", "sinusoidal", "brownian"
    float velocity_x = 0.0f;     // for linear
    float velocity_y = 0.0f;
    float amplitude_x = 0.0f;    // for sinusoidal
    float amplitude_y = 0.0f;
    float frequency_x = 0.0f;
    float frequency_y = 0.0f;
    float diffusion = 0.0f;      // for brownian
    uint64_t seed = 42;
    // Boundary behavior
    std::string boundary_mode = "clamp";  // "clamp", "bounce", "wrap"
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};
```

The motion model modifies the center position of a shape at each frame. Boundary modes control what happens at image edges.

#### 5c. Area-Driven Mask Generation

A special generator that takes an `AnalogTimeSeries` key (already in `DataManager`) and produces a `MaskData` whose area matches the time series values at each frame. The shape is a circle whose radius is derived from the desired area.

```cpp
struct AreaDrivenMaskParams {
    std::string area_signal_key;   // DataManager key for AnalogTimeSeries
    std::string shape = "circle";  // could be "circle", "square"
    int image_width = 640;
    int image_height = 480;
    float center_x = 320.0f;
    float center_y = 240.0f;
};
```

This closes the loop described in the motivation: generate an `AnalogTimeSeries` representing "mask area", then materialize it as actual mask data. The transform `CalculateMaskArea` can then be validated against the original signal.

**Milestone 5 exit criteria**: Static shapes generate correctly. Motion models produce time-varying positions respecting boundary constraints. Area-driven mask validates round-trip with `CalculateMaskArea` transform.

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
- **Multi-entity spatial generation**: Multiple independently moving masks/points in the same data object (e.g., simulating multiple tracked objects).
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
│  │  └─ DigitalInterval/
│  │     ├─ RegularIntervalsGenerator.cpp
│  │     └─ RandomIntervalsGenerator.cpp
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
    └── RandomIntervalsGenerator.test.cpp

docs/
└── developer/
    └── DataSynthesizer/
        ├── ROADMAP.md              (this file)
        ├── GeneratorRegistry.qmd   # ✅ Milestone 1
        ├── Widget.qmd              # ✅ Milestone 2
        └── Generators/
            ├── Analog.qmd          # ✅ Milestone 1
            ├── DigitalEvent.qmd    # ✅ Milestone 4
            └── DigitalInterval.qmd # ✅ Milestone 4
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

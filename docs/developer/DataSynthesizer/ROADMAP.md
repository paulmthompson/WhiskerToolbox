# Data Synthesizer — Development Roadmap

## Progress

| Milestone | Status | Date |
|-----------|--------|------|
| **1** — Core Library, Generators & Command | ✅ Complete | 2026-03-12 |
| **2** — GUI Widget (Skeleton, State, Preview) | ✅ Complete | 2026-03-12 |
| **3** — More AnalogTimeSeries Generators | 🔲 Not started | — |
| **4** — DigitalEventSeries & DigitalIntervalSeries Generators | ✅ Complete | 2026-03-14 |
| **5a** — Static Shape Generators | ✅ Complete | 2026-03-14 |
| **5b-i** — Trajectory Library + MovingPoint | ✅ Complete | 2026-03-14 |
| **5b-ia** — Enum & Variant Params for Motion Generators | ✅ Complete | 2026-03-14 |
| **5b-ii** — MovingMask + MovingLine Generators | ✅ Complete | 2026-03-16 |
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

- **Registry-based extensibility**: New generators register themselves statically (same `--whole-archive` pattern as TransformsV2). Adding a new waveform is a self-contained `.cpp` file with zero modifications to the registry or factory.
- **ParameterSchema-driven UI**: Every generator's params struct uses reflect-cpp. `AutoParamWidget` renders the form automatically. No hand-coded UI per generator.
- **Command Architecture integration**: A `SynthesizeData` command creates data in `DataManager` from a JSON descriptor — composable in pipelines, scriptable, and undo-able.
- **Deterministic by default**: Every stochastic generator requires a `seed` field. Same seed → same output, always.

---

## Completed Milestones

### Milestone 1 — Core Library, Generators & Command ✅ (2026-03-12)

`GeneratorRegistry` singleton with static self-registration via `RegisterGenerator<Params>`, five AnalogTimeSeries generators, and the `SynthesizeData` command.

- **Registry**: `src/DataSynthesizer/GeneratorRegistry.hpp`, `Registration.hpp`, `GeneratorTypes.hpp`
- **Generators**: `src/DataSynthesizer/Generators/Analog/` (SineWave, SquareWave, TriangleWave, GaussianNoise, UniformNoise)
- **Command**: `src/Commands/SynthesizeData.hpp` / `.cpp`
- **Docs**: `GeneratorRegistry.qmd`, `Generators/Analog.qmd`
- **Tests**: `tests/DataSynthesizer/` — one test file per generator, plus `GeneratorRegistry.test.cpp` and `SynthesizeDataCommand.test.cpp`

### Milestone 2 — GUI Widget ✅ (2026-03-12)

Dockable widget with generator selection, `AutoParamWidget` parameter form, OpenGL signal preview, and Generate button.

- **State**: `src/WhiskerToolbox/DataSynthesizer_Widget/Core/` (`DataSynthesizerState`, `DataSynthesizerStateData`)
- **Properties UI**: `src/WhiskerToolbox/DataSynthesizer_Widget/UI/DataSynthesizerProperties_Widget.*`
- **Preview**: `src/WhiskerToolbox/DataSynthesizer_Widget/UI/SynthesizerPreviewWidget.*` (uses `PlottingOpenGL::SceneRenderer`)
- **Registration**: `DataSynthesizerWidgetRegistration.*` — registered under `"View/Tools"`
- **Docs**: `Widget.qmd`

### Milestone 4 — DigitalEventSeries & DigitalIntervalSeries Generators ✅ (2026-03-14)

Six event/interval generators plus `GeneratorContext` infrastructure for generators that need `DataManager` access.

- **Generators**: `src/DataSynthesizer/Generators/DigitalEvent/` (PoissonEvents, RegularEvents, BurstEvents, InhomogeneousPoissonEvents) and `Generators/DigitalInterval/` (RegularIntervals, RandomIntervals)
- **Context**: `GeneratorContext` in `GeneratorTypes.hpp` — `GeneratorFunction` signature includes `GeneratorContext const &`; `RegisterGenerator` provides overloads for context-aware and context-free generators
- **Docs**: `Generators/DigitalEvent.qmd`, `Generators/DigitalInterval.qmd`
- **Tests**: `tests/DataSynthesizer/` — one test file per generator

**Future extension**: The `InhomogeneousPoissonEvents` generator reads a rate signal from DataManager via `rate_signal_key`. A future extension could allow specifying the rate signal as a nested generator descriptor, generating it on-the-fly.

### Milestone 5a — Static Shape Generators ✅ (2026-03-14)

Six static spatial generators: CircleMask, RectangleMask, EllipseMask (→ MaskData), GridPoints, RandomPoints (→ PointData), StraightLine (→ LineData).

- **Generators**: `src/DataSynthesizer/Generators/Mask/`, `Point/`, `Line/`
- **Docs**: `Generators/Spatial.qmd`
- **Tests**: `tests/DataSynthesizer/` — one test file per generator

### Milestone 5b-i — Trajectory Library + MovingPoint ✅ (2026-03-14)

Reusable trajectory library with three motion models (linear, sinusoidal, brownian) and three boundary modes (clamp, bounce, wrap). Validated via `MovingPoint` generator.

- **Trajectory library**: `src/DataSynthesizer/Trajectory/Trajectory.hpp` / `.cpp` — `TrajectoryParams`, `computeTrajectory()`. Boundary enforcement is per-step. Also provides `clipPixelsToImage()` in `PixelClipping.hpp`.
- **Generator**: `src/DataSynthesizer/Generators/Point/MovingPointGenerator.cpp`
- **Docs**: `Generators/MotionModels.qmd`
- **Tests**: `tests/DataSynthesizer/Trajectory.test.cpp`, `MovingPointGenerator.test.cpp`

### Milestone 5b-ia — Enum & Variant Params for Motion Generators ✅ (2026-03-14)

Refactored `MovingPointParams` to use `enum class BoundaryMode`, typed motion sub-structs (`LinearMotionParams`, `SinusoidalMotionParams`, `BrownianMotionParams`), and `rfl::TaggedUnion`-based `MotionModelVariant`. Eliminates the flat-optional UI in favor of combo-box-driven sub-forms showing only relevant fields.

- **Shared types**: `src/DataSynthesizer/Trajectory/MotionParams.hpp` — `BoundaryMode`, `BoundaryParams`, motion sub-structs, `MotionModelVariant`, `toTrajectoryParams()`
- **Docs**: Updated in `Generators/MotionModels.qmd`
- **Tests**: `MovingPointGenerator.test.cpp` — updated for new JSON format

---

## Open Milestones

### Milestone 3 — More AnalogTimeSeries Generators

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

Each generator is a self-contained `.cpp` in `src/DataSynthesizer/Generators/Analog/`, self-registering, with unit tests.

**Exit criteria**: All generators register, produce correct output sizes, are deterministic with seed, and appear in the GUI combo box automatically.

---

### Milestone 5b — Motion Models (remaining phases)

The trajectory → shape → populate pipeline design is documented in `Generators/MotionModels.qmd`. The trajectory library and shared motion param types (`MotionModelVariant`, `BoundaryParams`) are complete. The remaining phases apply this infrastructure to mask and line output types.

#### Shape Rendering by Output Type

| Output type | Reference point | Shape step | Boundary interaction |
|-------------|----------------|------------|---------------------|
| **PointData** (single) | The point itself | Identity — position *is* the shape | None |
| **PointData** (collection) | Collection origin | Rigid-body translate all points | None — individual points may leave bounds |
| **MaskData** | Shape center | Re-rasterize shape at position | **Clip** pixels to `[0, image_width) × [0, image_height)` via `clipPixelsToImage()` |
| **LineData** | Line centroid | Rigid-body translate all vertices | None — vertices are float-valued, renderer handles visibility |

#### 5b-ii. MovingMask + MovingLine Generators �

**Goal**: Apply the trajectory library to mask and line output types.

**Deliverables:**

1. ✅ **`MovingMask` generator** (`src/DataSynthesizer/Generators/Mask/MovingMaskGenerator.cpp`):
   - One generator, shape is a parameter: `shape = "circle" | "rectangle" | "ellipse"`.
   - Shape-specific fields: `radius` (circle), `width`/`height` (rectangle),
     `semi_major`/`semi_minor`/`angle` (ellipse). Unused fields are ignored.
   - `image_width`, `image_height`, `start_x`, `start_y`, `num_frames`, plus
     `MotionModelVariant` and `BoundaryMode` (flat fields, same pattern as `MovingPoint`).
   - At each frame: rasterize shape at trajectory position, clip to image bounds, store.
   - Uses `clipPixelsToImage()` from `Trajectory/PixelClipping.hpp`.
   - `MaskShape` enum defined in `namespace WhiskerToolbox::DataSynthesizer` (not
     anonymous namespace) because reflect-cpp requires named-namespace enums.
   - Tests: `MovingMaskGenerator.test.cpp` — shape types, area preservation,
     clipping, boundary modes, Brownian determinism, schema validation.

2. ✅ **`MovingLine` generator** (`src/DataSynthesizer/Generators/Line/MovingLineGenerator.cpp`):
   - Parameters: `start_x`, `start_y`, `end_x`, `end_y`, `num_points_per_line`, `num_frames`,
     `trajectory_start_x`, `trajectory_start_y`, plus `MotionModelVariant` and `BoundaryMode`
     from `MotionParams.hpp`.
   - Line shape defined once as offsets from centroid. At each frame, all vertices translated
     by trajectory offset. No clipping — vertices are float-valued.
   - Tests: `MovingLineGenerator.test.cpp` — centroid follows trajectory, relative vertex
     distances constant across frames, vertices may extend beyond bounds, boundary modes,
     Brownian determinism, schema validation, parameter rejection.

**Exit criteria**: Both generators registered. Mask clipping works at all edges. Line
translates rigidly. Tests pass. ✅

---

#### 5b-iii. Time-Varying Shape — Area-Driven Mask 🔲

**Goal**: Demonstrate the "shape varies with time" capability of the pipeline.

The trajectory says *where* the shape is, and an external `AnalogTimeSeries` says *how big*
it is at each frame. Both are independent inputs to the shape→populate step.

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

#### Multi-Entity Note

`RaggedTimeSeries` stores multiple entries per frame, so the data model supports
multiple objects naturally. Multi-entity is "compute N trajectories and render N shapes
per frame". The generator can accept a `count` field and derive starting positions from
a layout (e.g., evenly spaced, random). Deferred until there is a real need.

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

**Exit criteria**: `SynthesizeMultipleData` command works. Two correlated Gaussian signals have measured correlation within tolerance of the specified value.

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

**Exit criteria**: Fuzz tests pass. Golden-value tests pinned. Example pipelines execute without error.

---

### Milestone 8 — GUI Enhancements

**Goal**: Polish the GUI for power-user workflows.

| Feature | Description |
|---------|-------------|
| **Waveform superposition UI** | "Add another waveform" button that enables stacking generators (sine + noise). Internally generates a composite descriptor. |
| **Parameter presets** | Save/load named parameter sets (e.g., "60 Hz line noise", "Poisson λ=30 Hz"). Stored as JSON in a user-config directory. |
| **Batch generation** | Generate N signals at once with systematic parameter variation (e.g., sweep frequency from 1–100 Hz in 10 steps). |
| **Live preview update** | Auto-regenerate preview on parameter change (debounced). |
| **Drag-to-DataManager** | Drag the preview into the DataManager_Widget data tree to name and place it. |

---

### Future Directions (Not Scheduled)

- **Modulation**: AM/FM modulation of any periodic generator by another generator.
- **Convolution / filtering**: Apply FIR/IIR filters to generated signals.
- **Import external specifications**: Load generator configs from external tool formats (e.g., MATLAB struct descriptions).
- **Statistical summary widget**: Show autocorrelation, PSD, histogram of generated signal alongside a real signal for comparison.
- **TensorData generators**: Random matrices, identity, structured tensors for ML pipeline testing.
- **Replay / animation**: Step through time frames in the preview to see spatial data evolving.

---

## Key Design Decisions

### Why a separate `DataSynthesizer` library (not TransformsV2)?

Generators take *no input data* — they create data from parameters alone. While the registration mechanics are similar, the conceptual model is different enough to warrant a separate registry. This avoids awkward "null input" transforms and keeps the TransformsV2 type system clean.

### Why `rfl::Generic` for generator parameters in the command?

The `SynthesizeData` command doesn't know which generator will be used at compile time. The generator-specific params are deserialized inside the generator's factory function, using the same `rfl::json::read<SpecificParams>(params_json)` pattern as `CommandFactory`. This keeps the command interface stable while generators define their own parameter types.

### Why `--whole-archive`?

Static registration relies on side effects of constructing file-scope objects. If the linker sees no symbol references into a translation unit, it may discard it entirely, and the registration never happens. `--whole-archive` forces all object files in the static library to be linked. This is a known requirement also used by TransformsV2 tests.

### Why not put generators directly in the Command?

Separation of concerns. The `GeneratorRegistry` is usable from tests, benchmarks, and other non-command contexts. The command is just one consumer of the registry. For example, the GUI preview uses the registry directly without going through the command.

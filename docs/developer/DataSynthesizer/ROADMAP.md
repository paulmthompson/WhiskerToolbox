# Data Synthesizer — Development Roadmap

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
│  ┌──────────────────┐  ┌─────────────────┐              │
│  │ DataSynthesizer  │  │ Properties      │              │
│  │ State            │  │ Widget          │              │
│  │ (EditorState)    │  │ (AutoParamWidget│              │
│  │                  │  │  + preview)     │              │
│  └──────────────────┘  └─────────────────┘              │
└─────────────────────────────────────────────────────────┘
```

### Core Library: `DataSynthesizer` (no Qt dependency)

A new static library at `src/DataSynthesizer/` containing:

- **`GeneratorRegistry`** — Singleton mapping `(output_type, generator_name)` → factory function + `GeneratorMetadata` (parameter schema, description, category).
- **Generator interface** — A type-erased callable: `DataTypeVariant generate(rfl::Generic const& params)`. Each generator is a free function or stateless functor registered at static init time. No base class inheritance hierarchy needed — just a function signature.
- **Static registration macro/helper** — Similar to TransformsV2's `RegisterTransform`. A file-scope RAII object that calls `GeneratorRegistry::instance().register_(...)`.
- **Generator implementations** — One `.cpp` per generator (or per logical group). Self-registering.

### Command: `SynthesizeData`

A new `ICommand` in `src/DataManager/Commands/`:

```cpp
struct SynthesizeDataParams {
    std::string output_key;          // DataManager key for the result
    std::string generator_name;      // Registry lookup
    std::string output_type;         // "AnalogTimeSeries", "DigitalEventSeries", etc.
    rfl::Generic parameters;         // Generator-specific params (forwarded)
    std::string time_key = "time";   // TimeFrame association
};
```

`execute()` looks up the generator in the registry, calls it, stores the result via `DataManager::setData<T>()`.

### Widget: `DataSynthesizer_Widget`

A new widget at `src/WhiskerToolbox/DataSynthesizer_Widget/` following the TransformsV2_Widget pattern:

- **State** (`DataSynthesizerState : EditorState`) — Stores selected output type, selected generator, parameter JSON, output key.
- **Properties widget** — Output type selector (combo) → generator selector (combo, filtered by type) → `AutoParamWidget` (driven by the generator's `ParameterSchema`) → output key text field → "Generate" button → optional inline preview plot.

---

## Milestones

### Milestone 1 — Core Library + AnalogTimeSeries Generators + Command (no GUI)

**Goal**: A working `SynthesizeData` command that can produce `AnalogTimeSeries` from JSON, with a few initial generators to prove the pattern.

#### 1a. Generator Registry & Infrastructure

| Item | Description |
|------|-------------|
| `src/DataSynthesizer/CMakeLists.txt` | Static library target, depends on `DataManager`, `ParameterSchema`. No Qt. |
| `src/DataSynthesizer/GeneratorRegistry.hpp/cpp` | Singleton registry. Maps `(output_type, name)` → `GeneratorEntry{factory_fn, metadata}`. Provides `listGenerators(output_type)`, `generate(name, params)`, `getSchema(name)`. |
| `src/DataSynthesizer/GeneratorTypes.hpp` | `GeneratorMetadata` struct (name, description, category, output_type, parameter_schema). Type alias for the generator function signature. |
| `src/DataSynthesizer/Registration.hpp` | RAII registration helper (like TransformsV2's `RegisterTransform`). |
| Developer doc | `docs/developer/DataSynthesizer/GeneratorRegistry.qmd` |

#### 1b. Initial AnalogTimeSeries Generators

Each generator is a self-contained `.cpp` file under `src/DataSynthesizer/Generators/Analog/`:

| Generator | Params struct | Notes |
|-----------|--------------|-------|
| **SineWave** | `{num_samples, amplitude, frequency, phase, dc_offset}` | Basic periodic. Validates `num_samples > 0`, `frequency > 0`. |
| **SquareWave** | `{num_samples, amplitude, frequency, phase, dc_offset, duty_cycle}` | `duty_cycle` in `[0, 1]`, default `0.5`. |
| **TriangleWave** | `{num_samples, amplitude, frequency, phase, dc_offset}` | Linear ramps. |
| **GaussianNoise** | `{num_samples, mean, stddev, seed}` | i.i.d. Gaussian samples. |
| **UniformNoise** | `{num_samples, min_value, max_value, seed}` | i.i.d. Uniform samples. |

**Deliverables**:

- 5 generator `.cpp` files, each self-registering.
- Unit tests for each generator (determinism with seed, output size, value range).
- `docs/developer/DataSynthesizer/Generators/Analog.qmd`

#### 1c. `SynthesizeData` Command

| Item | Description |
|------|-------------|
| `src/DataManager/Commands/SynthesizeData.hpp/cpp` | `ICommand` implementation. Looks up generator, calls it, stores in `DataManager`. |
| `CommandFactory` registration | Add entry for `"SynthesizeData"`. |
| Unit tests | Round-trip JSON → command → verify DataManager contents. Determinism (same seed = same data). Unknown generator → error. |
| Developer doc | `docs/developer/DataManager/Commands/SynthesizeData.qmd` |

#### 1c'. Linker integration

Because generators use static registration (RAII objects in anonymous namespaces), the `SynthesizeData` command binary (and any test binary) **must** link the generator object files with `--whole-archive` / `-force_load` / `/WHOLEARCHIVE` to prevent the linker from discarding "unused" translation units. This is the same pattern used by TransformsV2 tests. The `DataSynthesizer` CMakeLists must expose a helper or clearly document this requirement.

**Milestone 1 exit criteria**: A test like this passes:

```cpp
TEST_CASE("SynthesizeData command produces deterministic sine wave") {
    auto dm = std::make_shared<DataManager>();
    CommandContext ctx{dm};

    auto cmd = CommandFactory::createCommandFromJson("SynthesizeData", R"({
        "output_key": "test_sine",
        "generator_name": "SineWave",
        "output_type": "AnalogTimeSeries",
        "parameters": {
            "num_samples": 1000,
            "amplitude": 2.0,
            "frequency": 0.01,
            "phase": 0.0,
            "dc_offset": 0.0
        }
    })");

    auto result = cmd->execute(ctx);
    REQUIRE(result.success);

    auto ts = dm->getData<AnalogTimeSeries>("test_sine");
    REQUIRE(ts != nullptr);
    REQUIRE(ts->getNumSamples() == 1000);
    // Verify first value ≈ 0.0 (sin(0))
    REQUIRE(ts->getAnalogDataAsFloat(TimeFrameIndex(0)) == Catch::Approx(0.0f));
}
```

---

### Milestone 2 — GUI Widget (Properties + Preview)

**Goal**: A dockable widget where users can select a generator, configure params, preview the output, and generate data into `DataManager`.

#### 2a. EditorState

| Item | Description |
|------|-------------|
| `DataSynthesizerState.hpp/cpp` | Stores: `output_type`, `generator_name`, `parameter_json`, `output_key`, `instance_id`. Serializable via reflect-cpp. |
| Module registration | `DataSynthesizerWidgetRegistration.hpp/cpp` — registers with `EditorRegistry` for docking. |

#### 2b. Properties Widget

| Item | Description |
|------|-------------|
| `DataSynthesizerProperties_Widget.hpp/cpp/ui` | Output type combo → generator combo → `AutoParamWidget` (schema from registry) → output key field → Generate button. |
| Wiring | Generate button calls `SynthesizeData` command via `OperationContext`. |
| State sync | Combo/field changes update `DataSynthesizerState`; state restoration repopulates UI. |

#### 2c. Inline Preview

| Item | Description |
|------|-------------|
| Preview panel | Embedded plot (using existing `DataViewer` infrastructure or a lightweight `QCustomPlot`/`JKQTPlotter` instance) showing the generated signal before committing to `DataManager`. |
| Workflow | User adjusts params → clicks "Preview" → plot updates → clicks "Generate" → data written to `DataManager`. Preview is ephemeral (not stored). |

#### 2d. Registration in MainWindow

| Item | Description |
|------|-------------|
| `mainwindow.cpp` | Add `DataSynthesizerWidgetModule::registerTypes(...)` call in `_registerEditorTypes()`. |
| Menu entry | Add under Modules menu. |

**Milestone 2 exit criteria**: User can open the Data Synthesizer panel, pick "AnalogTimeSeries" → "SineWave", set amplitude/frequency, preview a plot, and click Generate. The resulting signal appears in `DataManager_Widget` and can be viewed in `DataViewer`.

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

| Generator | Output Type | Params | Notes |
|-----------|------------|--------|-------|
| **PoissonEvents** | `DigitalEventSeries` | `{num_samples, lambda, seed}` | Homogeneous Poisson process. |
| **InhomogeneousPoissonEvents** | `DigitalEventSeries` | `{num_samples, rate_generator, seed}` | `rate_generator` is a nested generator spec (e.g., a SineWave descriptor) that produces λ(t). |
| **RegularEvents** | `DigitalEventSeries` | `{num_samples, interval, jitter_stddev, seed}` | Evenly spaced with optional Gaussian jitter. |
| **BurstEvents** | `DigitalEventSeries` | `{num_samples, burst_rate, within_burst_rate, burst_duration, seed}` | Clustered event patterns. |
| **RegularIntervals** | `DigitalIntervalSeries` | `{num_samples, on_duration, off_duration, start_offset}` | Periodic on/off. |
| **RandomIntervals** | `DigitalIntervalSeries` | `{num_samples, mean_duration, mean_gap, seed}` | Exponentially distributed durations and gaps. |

**Design note — Nested generator specs**: The `InhomogeneousPoissonEvents` generator introduces the concept of a generator that takes another generator's output as input. The params struct includes a `rate_generator` field which is itself a `{generator_name, parameters}` descriptor. The implementation generates the rate signal first (as a temporary `AnalogTimeSeries`), then uses it to drive the Poisson process. This pattern generalizes to any generator that needs a time-varying input.

**Milestone 4 exit criteria**: Event/interval generators work via command, appear in GUI, and the nested-generator pattern is validated for `InhomogeneousPoissonEvents`.

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

## File Layout (After Milestone 2)

```
src/
├── DataSynthesizer/
│   ├── CMakeLists.txt
│   ├── GeneratorRegistry.hpp
│   ├── GeneratorRegistry.cpp
│   ├── GeneratorTypes.hpp
│   ├── Registration.hpp
│   └── Generators/
│       └── Analog/
│           ├── SineWaveGenerator.cpp
│           ├── SquareWaveGenerator.cpp
│           ├── TriangleWaveGenerator.cpp
│           ├── GaussianNoiseGenerator.cpp
│           └── UniformNoiseGenerator.cpp
├── DataManager/
│   └── Commands/
│       ├── SynthesizeData.hpp
│       └── SynthesizeData.cpp
└── WhiskerToolbox/
    └── DataSynthesizer_Widget/
        ├── CMakeLists.txt
        ├── DataSynthesizerWidgetRegistration.hpp
        ├── DataSynthesizerWidgetRegistration.cpp
        ├── Core/
        │   ├── DataSynthesizerState.hpp
        │   └── DataSynthesizerState.cpp
        └── UI/
            ├── DataSynthesizerProperties_Widget.hpp
            ├── DataSynthesizerProperties_Widget.cpp
            └── DataSynthesizerProperties_Widget.ui

tests/
└── DataSynthesizer/
    ├── CMakeLists.txt
    ├── GeneratorRegistry.test.cpp
    ├── SineWaveGenerator.test.cpp
    ├── SquareWaveGenerator.test.cpp
    ├── TriangleWaveGenerator.test.cpp
    ├── GaussianNoiseGenerator.test.cpp
    ├── UniformNoiseGenerator.test.cpp
    └── SynthesizeDataCommand.test.cpp

docs/
└── developer/
    └── DataSynthesizer/
        ├── ROADMAP.md              (this file)
        ├── GeneratorRegistry.qmd
        └── Generators/
            └── Analog.qmd
```

## Implementation Order (Milestone 1 detailed)

For the first milestone, the recommended implementation sequence is:

1. **`GeneratorTypes.hpp`** — Define `GeneratorMetadata`, `GeneratorFunction` type alias, `GeneratorEntry`.
2. **`GeneratorRegistry.hpp/cpp`** — Singleton with `register_()`, `generate()`, `listGenerators()`, `getSchema()`.
3. **`Registration.hpp`** — RAII helper struct for static registration.
4. **`SineWaveGenerator.cpp`** — First generator. Validates the full registration → generation → output path.
5. **Unit tests for SineWave** — Proves the registry works end-to-end.
6. **`SynthesizeData` command** — Wires generators to `DataManager` via command architecture.
7. **Command unit tests** — Round-trip JSON → command → DataManager verification.
8. **Remaining generators** (Square, Triangle, Gaussian, Uniform) — Now that the pattern is proven, these are mechanical.
9. **CMake integration** — `--whole-archive` linking for test binaries.
10. **Developer documentation**.

## Key Design Decisions

### Why a separate `DataSynthesizer` library (not TransformsV2)?

TransformsV2 transforms take input data and produce output data. Generators take *no input data* — they create data from parameters alone. While the registration mechanics are similar, the conceptual model is different enough to warrant a separate registry. This avoids awkward "null input" transforms and keeps the TransformsV2 type system clean.

### Why `rfl::Generic` for generator parameters in the command?

The `SynthesizeData` command doesn't know which generator will be used at compile time. The generator-specific params are deserialized inside the generator's factory function, using the same `rfl::json::read<SpecificParams>(params_json)` pattern as `CommandFactory`. This keeps the command interface stable while generators define their own parameter types.

### Why `--whole-archive`?

Static registration relies on side effects of constructing file-scope objects. If the linker sees no symbol references into a translation unit, it may discard it entirely, and the registration never happens. `--whole-archive` forces all object files in the static library to be linked. This is a known requirement also used by TransformsV2 tests.

### Why not put generators directly in the Command?

Separation of concerns. The `GeneratorRegistry` is usable from tests, benchmarks, and other non-command contexts. The command is just one consumer of the registry. For example, the GUI preview uses the registry directly without going through the command.

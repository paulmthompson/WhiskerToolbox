# Data Synthesizer — Development Roadmap

## Progress

| Milestone | Status | Date |
|-----------|--------|------|
| **1** — Core Library, Generators & Command | ✅ Complete | 2026-03-12 |
| **2a** — Widget Skeleton & Registration | 🔲 Not started | — |
| **2b** — State & AutoParamWidget Integration | 🔲 Not started | — |
| **2c** — OpenGL Signal Preview | 🔲 Not started | — |
| **3** — More AnalogTimeSeries Generators | 🔲 Not started | — |
| **4** — DigitalEventSeries & DigitalIntervalSeries Generators | 🔲 Not started | — |
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

Milestone 1 delivered the `DataSynthesizer` static library, five `AnalogTimeSeries` generators, and the `SynthesizeData` command. Summary:

- **GeneratorRegistry** (`src/DataSynthesizer/`): Singleton mapping generator names → factory functions + metadata. Generators self-register via `RegisterGenerator<Params>` RAII helper at static init (same `--whole-archive` pattern as TransformsV2). `GeneratorFunction` takes a JSON string; `RegisterGenerator` handles deserialization via `rfl::json::read<Params>`.
- **Five AnalogTimeSeries generators** (`src/DataSynthesizer/Generators/Analog/`): SineWave, SquareWave, TriangleWave, GaussianNoise, UniformNoise. Each is a self-contained `.cpp` with a params struct, validation, and static registration.
- **`SynthesizeData` command** (`src/Commands/SynthesizeData.hpp/cpp`): `ICommand` that looks up a generator by name, passes `rfl::Generic` parameters (serialized to JSON string), stores the result via `DataManager::setData()`. Category `"data_generation"`. Tests verify round-trip JSON → command → DataManager, determinism, and error handling.
- **Linker integration**: `Commands` library links `DataSynthesizer` privately. Test binaries use `--whole-archive` / `-force_load` / `/WHOLEARCHIVE` for static registration.
- **Documentation**: `GeneratorRegistry.qmd`, `Analog.qmd`, `synthesize_data.qmd`, and `_quarto.yml`/`copilot-instructions.md` updates.

---

## Milestone 2 — GUI Widget

**Goal**: A dockable widget where users can select a generator, configure params, preview the output, and generate data into `DataManager`.

### 2a. Widget Skeleton & Registration

**Goal**: Create blank state, properties, and view panels. Register the widget so it can be raised from the MainWindow menu.

#### Deliverables

| Item | Description |
|------|-------------|
| `DataSynthesizerState.hpp/cpp` | Minimal `EditorState` subclass. Contains `DataSynthesizerStateData` with `instance_id` and `display_name` only. Implements `getTypeName()` → `"DataSynthesizerWidget"`, `toJson()`/`fromJson()` via reflect-cpp. |
| `DataSynthesizerStateData.hpp` | reflect-cpp serializable POD struct: `{instance_id, display_name}`. |
| `DataSynthesizerProperties_Widget.hpp/cpp` | Empty `QWidget` subclass with a `QVBoxLayout`. Accepts `shared_ptr<DataSynthesizerState>` in constructor. Displays a placeholder `QLabel("Data Synthesizer — Properties")`. |
| `DataSynthesizerView_Widget.hpp/cpp` | Empty `QWidget` subclass with a `QVBoxLayout`. Accepts `shared_ptr<DataSynthesizerState>` and `shared_ptr<DataManager>`. Displays a placeholder `QLabel("Data Synthesizer — Preview")`. This will later hold the OpenGL preview. |
| `DataSynthesizerWidgetRegistration.hpp/cpp` | `DataSynthesizerWidgetModule::registerTypes(EditorRegistry*, shared_ptr<DataManager>)`. Registers type `"DataSynthesizerWidget"` with `display_name = "Data Synthesizer"`, `menu_path = "View/Tools"`, `preferred_zone = Zone::Right`, `properties_zone = Zone::Right`, `allow_multiple = false`. Uses `create_editor_custom` to create state, properties, and view widgets. The view goes to the center/right zone; properties as a tab in the right zone. |
| `CMakeLists.txt` | Static library `DataSynthesizer_Widget`, depends on `Qt6::Widgets`, `EditorState`, `DataManager`. |
| `mainwindow.cpp` change | Add `DataSynthesizerWidgetModule::registerTypes(_editor_registry.get(), _data_manager)` call in `_registerEditorTypes()`. Add `#include` for registration header. |

#### File Layout

```
src/WhiskerToolbox/DataSynthesizer_Widget/
├── CMakeLists.txt
├── DataSynthesizerWidgetRegistration.hpp
├── DataSynthesizerWidgetRegistration.cpp
├── Core/
│   ├── DataSynthesizerState.hpp
│   ├── DataSynthesizerState.cpp
│   └── DataSynthesizerStateData.hpp
└── UI/
    ├── DataSynthesizerProperties_Widget.hpp
    ├── DataSynthesizerProperties_Widget.cpp
    ├── DataSynthesizerView_Widget.hpp
    └── DataSynthesizerView_Widget.cpp
```

#### Exit Criteria

User can launch the Data Synthesizer from the View/Tools menu. Two blank panels appear (properties and preview). Closing and reopening the widget restores state (instance ID and display name round-trip through JSON).

---

### 2b. State & AutoParamWidget Integration

**Goal**: Populate `DataSynthesizerState` with the fields needed for generator selection and parameter editing. Wire the properties widget to use `AutoParamWidget` for rendering generator-specific parameter forms.

#### State Expansion

`DataSynthesizerStateData` gains:

```cpp
struct DataSynthesizerStateData {
    std::string instance_id;
    std::string display_name = "Data Synthesizer";

    // Generator selection
    std::string output_type;       // "AnalogTimeSeries", "DigitalEventSeries", etc.
    std::string generator_name;    // Registry key (e.g., "SineWave")
    std::string parameter_json;    // Current params as JSON string

    // Output configuration
    std::string output_key;        // DataManager key for generated data
    std::string time_key = "time"; // TimeFrame association
};
```

`DataSynthesizerState` adds getters/setters with `markDirty()` + signals for each new field:
- `setOutputType()` / `outputType()` + `outputTypeChanged()`
- `setGeneratorName()` / `generatorName()` + `generatorNameChanged()`
- `setParameterJson()` / `parameterJson()` + `parameterJsonChanged()`
- `setOutputKey()` / `outputKey()` + `outputKeyChanged()`

#### Properties Widget UI

`DataSynthesizerProperties_Widget` is rebuilt with these sections (top to bottom):

1. **Output Type combo** (`QComboBox`): Populated with the unique `output_type` values from `GeneratorRegistry::instance().listAllGenerators()`. Changing this filters the generator combo.

2. **Generator combo** (`QComboBox`): Populated with generator names filtered by the selected output type via `GeneratorRegistry::instance().listGenerators(output_type)`. Changing this:
   - Fetches the `ParameterSchema` from the registry via `getSchema(generator_name)`.
   - Destroys any existing `AutoParamWidget` and creates a new one with the schema.
   - Resets `parameter_json` in state.

3. **AutoParamWidget** (dynamically created): Renders the generator's parameter form. Connected via `AutoParamWidget::parametersChanged` → `state->setParameterJson(widget->toJson())`.

4. **Output key** (`QLineEdit`): DataManager key for the generated data. Default: generator name in snake_case (e.g., `"sine_wave_1"`).

5. **Generate button** (`QPushButton`): Constructs a `SynthesizeData` command from state fields and executes it via `CommandContext`. On success, emits a status message.

#### State ↔ UI Sync

- **UI → State**: Combo/field changes call the corresponding state setter. `AutoParamWidget::parametersChanged` updates `parameter_json`.
- **State → UI** (restore): `fromJson()` emits all change signals. Properties widget connects to these signals and repopulates combos, recreates `AutoParamWidget` with stored schema, and calls `fromJson()` on it with the stored `parameter_json`.

#### CMake Changes

`DataSynthesizer_Widget` adds dependencies: `AutoParamWidget`, `WhiskerToolbox::DataSynthesizer`, `ParameterSchema`. Must link `DataSynthesizer` with `--whole-archive` for static registration.

#### Exit Criteria

1. User opens the Data Synthesizer panel.
2. Selects "AnalogTimeSeries" from the output type combo.
3. Generator combo shows: SineWave, SquareWave, TriangleWave, GaussianNoise, UniformNoise.
4. Selecting "SineWave" displays parameter form: num_samples, amplitude, frequency, phase, dc_offset fields.
5. Changing generator replaces the parameter form with the new generator's fields.
6. Clicking "Generate" with valid params creates an `AnalogTimeSeries` in DataManager (visible in DataManager_Widget).
7. Closing and reopening the widget restores the selected output type, generator, parameters, and output key.

---

### 2c. OpenGL Signal Preview

**Goal**: Embed a lightweight `QOpenGLWidget` in the view panel that renders the generated `AnalogTimeSeries` using the `CorePlotting` + `PlottingOpenGL` rendering pipeline. Users click "Preview" to see the signal before committing it to DataManager.

#### Architecture

The preview uses the same rendering stack as `DataViewer_Widget` but in a minimal, non-interactive configuration:

```
AnalogTimeSeries (ephemeral, from generator)
        │
        ▼
TimeSeriesMapper::mapAnalogSeriesFull()  →  MappedVertex range
        │
        ▼
SceneBuilder::addPolyLine()              →  RenderableScene
        │
        ▼
SceneRenderer::uploadScene() + render()  →  OpenGL output
```

No interaction handling, no selection, no multi-series support, no scrolling — just a single static polyline plot.

#### Deliverables

| Item | Description |
|------|-------------|
| `SynthesizerPreviewWidget.hpp/cpp` | `QOpenGLWidget` subclass. Holds a `PlottingOpenGL::SceneRenderer`, a cached `CorePlotting::RenderableScene`, and optionally a local `AnalogTimeSeries` + `TimeFrame`. Implements `initializeGL()`, `paintGL()`, `resizeGL()`. Provides `setData(AnalogTimeSeries)` to upload new preview data. |
| Properties widget "Preview" button | New `QPushButton` in properties panel. Calls the generator directly via `GeneratorRegistry::generate()` (no DataManager involvement). If the result is an `AnalogTimeSeries`, passes it to `SynthesizerPreviewWidget::setData()`. |
| View widget integration | `DataSynthesizerView_Widget` replaces its placeholder label with a `SynthesizerPreviewWidget` embedded via layout. |
| CMake changes | `DataSynthesizer_Widget` adds dependencies: `PlottingOpenGL`, `CorePlotting`, `Qt6::OpenGL`, `Qt6::OpenGLWidgets`, `glm::glm`. |

#### `SynthesizerPreviewWidget` Design

```cpp
class SynthesizerPreviewWidget : public QOpenGLWidget {
public:
    explicit SynthesizerPreviewWidget(QWidget * parent = nullptr);

    /// @brief Set a new AnalogTimeSeries for preview. Rebuilds the scene and triggers repaint.
    void setData(std::shared_ptr<AnalogTimeSeries> series);

    /// @brief Clear the preview.
    void clearData();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void rebuildScene();

    std::unique_ptr<PlottingOpenGL::SceneRenderer> _scene_renderer;
    std::shared_ptr<AnalogTimeSeries> _series;

    // View parameters (computed from data range)
    float _x_min = 0.0f;
    float _x_max = 1.0f;
    float _y_min = -1.0f;
    float _y_max = 1.0f;
};
```

**Scene building** (`rebuildScene()`):
1. Compute data bounds: `x_min = 0`, `x_max = num_samples - 1`, `y_min/y_max` from data min/max with 10% padding.
2. Create an identity `SeriesLayout` (no offset/gain — raw data coordinates).
3. Call `TimeSeriesMapper::mapAnalogSeriesFull()` to get vertices.
4. Use `SceneBuilder::addPolyLine()` with a white or blue `PolyLineStyle` to build the scene.
5. Set view/projection matrices:
   - View matrix: identity.
   - Projection matrix: `glm::ortho(x_min, x_max, y_min, y_max, -1.0f, 1.0f)`.
6. Store the built `RenderableScene`.

**Rendering** (`paintGL()`):
1. Clear background to dark gray.
2. Upload and render the cached scene via `SceneRenderer`.

**Resize** (`resizeGL()`):
1. Update `glViewport`.
2. Rebuild scene with updated projection if data exists.

#### Properties Widget "Preview" Button Wiring

```
User clicks "Preview"
    │
    ├─ Read generator_name + parameter_json from state
    ├─ Call GeneratorRegistry::generate(generator_name, parameter_json)
    ├─ If result holds AnalogTimeSeries:
    │      Extract from DataTypeVariant
    │      Call _view_widget->previewWidget()->setData(series)
    └─ If result is error or wrong type:
           Show error in status label
```

The preview is **ephemeral** — it lives only in the widget, not in DataManager. The "Generate" button separately creates the data in DataManager via the `SynthesizeData` command.

#### Exit Criteria

1. User selects SineWave generator with `num_samples=500, amplitude=2.0, frequency=0.02`.
2. Clicks "Preview" → the OpenGL panel draws a smooth sine waveform.
3. Changing parameters and clicking "Preview" again updates the plot.
4. Clicking "Generate" writes the data to DataManager without affecting the preview.
5. Preview renders correctly on resize.
6. Preview can be cleared when switching to a non-analog output type.

---

### Milestone 2 overall exit criteria

User can open the Data Synthesizer from View/Tools, select a generator type and generator, configure parameters via auto-generated form, preview the analog signal in the embedded OpenGL plot, and generate the data into DataManager. State persists across widget close/reopen.

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
├── DataSynthesizer/                          # ✅ Milestone 1 (complete)
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
├── Commands/                                 # ✅ Milestone 1 (complete)
│   ├── SynthesizeData.hpp
│   └── SynthesizeData.cpp
└── WhiskerToolbox/
    └── DataSynthesizer_Widget/               # Milestone 2
        ├── CMakeLists.txt                       # 2a
        ├── DataSynthesizerWidgetRegistration.hpp # 2a
        ├── DataSynthesizerWidgetRegistration.cpp # 2a
        ├── Core/
        │   ├── DataSynthesizerStateData.hpp      # 2a (minimal), 2b (expanded)
        │   ├── DataSynthesizerState.hpp           # 2a (minimal), 2b (expanded)
        │   └── DataSynthesizerState.cpp           # 2a (minimal), 2b (expanded)
        └── UI/
            ├── DataSynthesizerProperties_Widget.hpp  # 2a (stub), 2b (functional)
            ├── DataSynthesizerProperties_Widget.cpp  # 2a (stub), 2b (functional)
            ├── DataSynthesizerView_Widget.hpp         # 2a (stub), 2c (OpenGL)
            ├── DataSynthesizerView_Widget.cpp         # 2a (stub), 2c (OpenGL)
            ├── SynthesizerPreviewWidget.hpp           # 2c
            └── SynthesizerPreviewWidget.cpp           # 2c

tests/
└── DataSynthesizer/                          # ✅ Milestone 1 (complete)
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
        ├── GeneratorRegistry.qmd   # ✅ Milestone 1
        ├── Widget.qmd              # Milestone 2
        └── Generators/
            └── Analog.qmd         # ✅ Milestone 1
```

## Implementation Order (Milestone 2 detailed)

For Milestone 2, the recommended implementation sequence is:

### 2a — Widget Skeleton (4 items)
1. **`DataSynthesizerStateData.hpp`** — Minimal POD struct: `{instance_id, display_name}`.
2. **`DataSynthesizerState.hpp/cpp`** — `EditorState` subclass with `toJson()`/`fromJson()`.
3. **`DataSynthesizerProperties_Widget.hpp/cpp`** — Empty `QWidget` with placeholder label.
4. **`DataSynthesizerView_Widget.hpp/cpp`** — Empty `QWidget` with placeholder label.
5. **`DataSynthesizerWidgetRegistration.hpp/cpp`** — Register with `EditorRegistry`.
6. **`CMakeLists.txt`** — Static library, link `Qt6::Widgets`, `EditorState`, `DataManager`.
7. **`mainwindow.cpp`** — Add registration call in `_registerEditorTypes()`.
8. **Build & verify** — Widget appears in View/Tools menu.

### 2b — State & AutoParamWidget (3 items)
1. **Expand `DataSynthesizerStateData`** — Add `output_type`, `generator_name`, `parameter_json`, `output_key`, `time_key`.
2. **Expand `DataSynthesizerState`** — Getters/setters with signals for each new field.
3. **Rebuild properties widget UI** — Output type combo → generator combo → `AutoParamWidget` → output key → Generate button.
4. **CMake: add dependencies** — `AutoParamWidget`, `DataSynthesizer` (with `--whole-archive`), `ParameterSchema`.
5. **Build & verify** — Generator selection drives parameter form; Generate creates data in DataManager.

### 2c — OpenGL Preview (3 items)
1. **`SynthesizerPreviewWidget.hpp/cpp`** — `QOpenGLWidget` with `SceneRenderer`, identity layout, `mapAnalogSeriesFull()`, `glm::ortho()` projection.
2. **Integrate into view widget** — Replace placeholder label with `SynthesizerPreviewWidget`.
3. **Add Preview button** — Properties widget calls `GeneratorRegistry::generate()` directly, passes result to preview.
4. **CMake: add dependencies** — `PlottingOpenGL`, `CorePlotting`, `Qt6::OpenGL`, `Qt6::OpenGLWidgets`, `glm::glm`.
5. **Build & verify** — Preview renders sine wave correctly.

## Key Design Decisions

### Why a separate `DataSynthesizer` library (not TransformsV2)?

TransformsV2 transforms take input data and produce output data. Generators take *no input data* — they create data from parameters alone. While the registration mechanics are similar, the conceptual model is different enough to warrant a separate registry. This avoids awkward "null input" transforms and keeps the TransformsV2 type system clean.

### Why `rfl::Generic` for generator parameters in the command?

The `SynthesizeData` command doesn't know which generator will be used at compile time. The generator-specific params are deserialized inside the generator's factory function, using the same `rfl::json::read<SpecificParams>(params_json)` pattern as `CommandFactory`. This keeps the command interface stable while generators define their own parameter types.

### Why `--whole-archive`?

Static registration relies on side effects of constructing file-scope objects. If the linker sees no symbol references into a translation unit, it may discard it entirely, and the registration never happens. `--whole-archive` forces all object files in the static library to be linked. This is a known requirement also used by TransformsV2 tests.

### Why not put generators directly in the Command?

Separation of concerns. The `GeneratorRegistry` is usable from tests, benchmarks, and other non-command contexts. The command is just one consumer of the registry. For example, the GUI preview uses the registry directly without going through the command.

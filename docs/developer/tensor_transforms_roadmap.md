# Tensor Transform Library — Design Roadmap

**Last updated:** 2026-03-21

## Motivation

TensorData is the richest numeric container in WhiskerToolbox, supporting 2D matrices, 3D cubes, and N-dimensional arrays across multiple storage backends (Armadillo, Dense, LibTorch). Many scientific workflows require **tensor-in → tensor-out** operations: dimensionality reduction (PCA, t-SNE, UMAP), spectral decomposition (SVD, NMF), normalization, slicing, and reshaping. These transforms differ from existing element-level transforms (which operate on a single `Mask2D` or `Line2D`) because:

1. **Shape-dependence** — A PCA requires a 2D matrix; a conv-filter might need 3D. The transform's validity depends on the tensor's dimensionality.
2. **Whole-container semantics** — Dimensionality reduction operates on the *entire* matrix, not element-by-element. There is no natural "element" to lift.
3. **Output shape differs from input** — PCA(N×D) → (N×K) where K < D. The transform changes the column dimension.
4. **Shared state** — Some algorithms produce a fitted model (PCA eigenvectors, t-SNE embedding) that may be reused for projection of new data.

This document evaluates architectural options, recommends a design, and provides an implementation roadmap.

---

## Target Workflow

The primary motivating use case is the **encoder → embedding → visualization → interaction** loop:

```
Video (frames)
  │  DeepLearning_Widget: encoder + global average pooling
  ▼
TensorData  (N×192, TimeFrameIndex rows, "encoder_features")
  │  DataTransform_Widget: TensorPCA or TensorTSNE
  ▼
TensorData  (N×2, TimeFrameIndex rows, columns "PC1"/"PC2")
  │  ScatterPlotWidget: X=PC1, Y=PC2, color-by-group
  ▼
Scatter visualization with interactive selection
  │  User selects visible cluster → entity group created
  ▼
EntityGroupManager  (group "cluster_0" → {EntityId_1, EntityId_2, ...})
  │  MLCore_Widget or manual grouping
  ▼
Downstream analysis (classification, interval generation, export)
```

### What this workflow requires

| Step | Component | Status |
|------|-----------|--------|
| Encode video → TensorData | DeepLearning_Widget + ResultProcessor | **Exists** |
| PCA / t-SNE on TensorData | MLCore + TransformsV2 wrapper | **This roadmap** |
| Scatter plot of reduced features | ScatterPlotWidget | **Exists** — supports TensorData columns on X/Y axes |
| Color points by group | ScatterPlotWidget | **Exists** — `colorByGroup` + `GroupManager` integration |
| Select points in scatter | ScatterPlotWidget | **Exists** — single-point (Ctrl+Click) and polygon selection |
| Create group from selection | ScatterPlotWidget → EntityGroupManager | **Done** — Phase 2 ✅ |
| Cluster automatically | MLCore_Widget (ClusteringPipeline) | **Exists** — but needs scatter↔MLCore coordination |
| Propagate groups to other widgets | SelectionContext + DataFocusAware | **Exists** — entity selection broadcasts to all listeners |

### Cross-Widget Interaction Gaps

> **See also:** [Inter-Widget Communication Roadmap](inter_widget_communication_roadmap.md) for a broader analysis of communication patterns, including the "Context-Aware Actions" proposal that would reduce manual steps for gaps 1 and 3 below.

The existing widget communication infrastructure (`SelectionContext`, `DataFocusAware`, `EntityGroupManager`, `ObserverData`) is sufficient for loose coupling. No widget needs a pointer to another. However, several **specific gaps** must be closed:

1. **ScatterPlotWidget → EntityGroupManager (selection → group creation):**
   The scatter widget can select points and knows their `TimeFrameIndex` values, but has no built-in action to create a named entity group from the current selection. The scatter widget currently receives a `GroupManager*` for *reading* group colors, but the "create group from selection" action needs to be added. This should be a context-menu action that:
   - Collects `TimeFrameIndex` values from selected points
   - Maps them to `EntityId` via the scatter widget's data key + `EntityRegistry`
   - Creates a new group in `EntityGroupManager`
   - The group creation triggers observer notifications → other widgets update

2. **MLCore_Widget cluster output → ScatterPlotWidget color refresh:**
   When `ClusteringPipeline` writes entity groups, `EntityGroupManager` notifies observers. The scatter widget already listens to group changes via a generation counter and marks the scene dirty. **This path works today** — no new wiring needed. The user would: (a) focus the reduced TensorData, (b) run clustering in MLCore_Widget, (c) scatter automatically recolors.

3. **DataTransform_Widget → ScatterPlotWidget data flow:**
   After the DataTransform_Widget executes a PCA transform, the result TensorData lands in DataManager. The scatter widget's properties panel auto-refreshes its key combo boxes via its DataManager observer callback. The user selects the new tensor and its columns. **This path works today.** A quality-of-life improvement would be auto-focusing the output key via `SelectionContext::setDataFocus()` after transform execution.

4. **Row time preservation through transforms:**
   The PCA transform must preserve `RowDescriptor` from the input tensor. If the input has `TimeFrameIndex` rows (because the encoder ran on video frames), the output must also have `TimeFrameIndex` rows with the same indices. This is critical for:
   - ScatterPlotWidget to join X/Y axes by time
   - ScatterPlotWidget to map selected points back to `TimeFrameIndex` → `EntityId`
   - Navigation (double-click point → jump to frame)

5. **TensorData column naming for scatter axis auto-configuration:**
   When a PCA produces a tensor with columns `["PC1", "PC2"]`, the scatter properties widget should list these as axis options. The scatter widget already supports `tensor_column_name` in `ScatterAxisSource`. No new work needed — the PCA transform just needs to produce named columns.

---

## Current Architecture Landscape

### TransformsV2 System
- **Element transforms** `In → Out` operate on single data items (e.g., `Mask2D → float`). Automatically lifted to containers via `ContainerTraits`.
- **Container transforms** `shared_ptr<Out>(InContainer const&, Params, ComputeContext)` have full access to the container and produce a new container. Already used for `AnalogTimeSeries → DigitalEventSeries`, and critically, **binary container transforms already produce TensorData** (e.g., `AnalogIntervalReduction`).
- **Registration** is static (RAII in anonymous namespaces) and uses `--whole-archive` linking.
- **ParameterSchema** provides automatic UI generation for any reflect-cpp parameter struct.

### MLCore System
- Wraps mlpack algorithms behind the `MLModelOperation` interface: `fit()`, `predict()`, `assignClusters()`.
- Uses `arma::mat` as the data interchange format (features × observations, column-major).
- Has `ClassificationPipeline` and `ClusteringPipeline` that orchestrate feature conversion, model fitting, and output writing.
- Currently implements: KMeans, DBSCAN, GaussianMixture (unsupervised); LogisticRegression, NaiveBayes, HMM, RandomForest (supervised).

### TensorData
- Factory methods: `createOrdinal2D`, `createTimeSeries2D`, `createFromIntervals`, `createFromArmadillo`, `createND`.
- Direct Armadillo access via `asArmadilloMatrix()` (2D) and `asArmadilloCube()` (3D).
- Row semantics via `RowDescriptor`: Ordinal, TimeFrameIndex, or Interval.
- Column names supported for labeled features.

---

## Design Options

### Option A: Register as TransformsV2 Container Transforms

**Pattern:** `TensorData → TensorData` container transforms, registered in `ElementRegistry`.

```
src/TransformsV2/algorithms/
├── TensorPCA/
│   ├── TensorPCA.hpp
│   ├── TensorPCA.cpp
│   └── TensorPCA_registration.cpp   // RAII static registration
├── TensorTSNE/
│   ├── TensorTSNE.hpp
│   ├── TensorTSNE.cpp
│   └── TensorTSNE_registration.cpp
...
```

**Pros:**
- Fully integrated into the existing pipeline system (JSON pipelines, parameter binding, UI discovery).
- Automatic parameter schema extraction and AutoParamWidget form generation.
- Composable with other transforms in multi-step pipelines (e.g., IntervalReduction → PCA → KMeans).
- Consistent developer experience — same registration pattern, same execution path.
- No new libraries or build targets needed.

**Cons:**
- TransformsV2 is already a large compilation unit. Adding tapkee (header-only, heavy template usage) would further increase build times.
- Container transforms are pure functions `(Input, Params, Ctx) → Output`, whereas dimensionality reduction algorithms have a natural *fit/transform* duality (fit on training data, apply to new data). A pure-function signature discards the fitted model.
- Doesn't naturally support the "fit once, project many" workflow.

**Mitigation for fit/transform:** The container transform can eagerly fit+transform in a single call (same as how sklearn's `fit_transform()` works). For the "project new data" use case, a separate `TensorProject` transform could accept the fitted model as a serialized parameter or via PipelineValueStore.

---

### Option B: New `TensorTransforms` Static Library

**Pattern:** A standalone library that depends on TensorData + tapkee, with its own registry.

```
src/TensorTransforms/
├── CMakeLists.txt
├── TensorTransformRegistry.hpp       // Singleton registry
├── TensorTransformOperation.hpp      // Base class (like MLModelOperation)
├── algorithms/
│   ├── PCA.hpp / PCA.cpp
│   ├── TSNE.hpp / TSNE.cpp
│   ├── UMAP.hpp / UMAP.cpp
│   └── NMF.hpp / NMF.cpp
└── adapters/
    └── TransformsV2Adapter.cpp       // Optional bridge registration
```

**Pros:**
- Clean separation of concerns — tapkee dependency isolated to one library.
- Natural fit/transform API without contorting to the single-function container transform pattern.
- Can expose a richer interface (fitted model inspection, explained variance, etc.).
- Faster incremental builds when modifying tensor algorithms (doesn't recompile TransformsV2).

**Cons:**
- New library to maintain. Duplicates registry infrastructure unless carefully integrated.
- Not automatically discoverable by the pipeline system or TransformsV2 widget unless a bridge is written.
- Risks creating a parallel universe of transform infrastructure.

---

### Option C: Extend MLCore with Dimensionality Reduction Operations

**Pattern:** Add PCA/t-SNE/UMAP as `MLModelOperation` subclasses in MLCore.

```
src/MLCore/models/unsupervised/
├── PCAOperation.hpp / PCAOperation.cpp
├── TSNEOperation.hpp / TSNEOperation.cpp
└── UMAPOperation.hpp / UMAPOperation.cpp
```

**Pros:**
- `MLModelOperation` already has `fit()` + `predict()` semantics that map naturally to fit/transform.
- Existing `ClusteringPipeline` pattern could be extended to a `DimensionalityReductionPipeline`.
- MLCore already manages the TensorData ↔ Armadillo conversion via `FeatureConverter`.
- Keeps all ML/statistical algorithms in one place.

**Cons:**
- MLCore currently depends on mlpack. Adding tapkee introduces a second ML backend.
- `MLModelOperation` interface is biased toward classification/clustering (labels, assignments, probabilities). Dimensionality reduction returns a *transformed matrix*, which doesn't fit `predict()` or `assignClusters()` cleanly.
- Still needs a TransformsV2 bridge for pipeline integration.

---

### Option D (Recommended): Hybrid — MLCore Operations + TransformsV2 Container Adapters

**Pattern:** Implement the algorithms in MLCore with a clean `DimensionalityReductionOperation` base class, then register thin TransformsV2 container transforms that call into MLCore.

```
# MLCore — Algorithm implementations
src/MLCore/
├── models/
│   ├── MLDimReductionOperation.hpp    // New base class
│   └── unsupervised/
│       ├── PCAOperation.hpp / .cpp
│       ├── TSNEOperation.hpp / .cpp
│       └── UMAPOperation.hpp / .cpp

# TransformsV2 — Thin registration wrappers
src/TransformsV2/algorithms/
├── TensorPCA/
│   ├── TensorPCA.hpp         // Params struct (reflect-cpp)
│   ├── TensorPCA.cpp         // Calls into MLCore PCAOperation
│   └── (static registration in RegisteredTransforms.cpp)
├── TensorTSNE/
│   └── ...
```

**Pros:**
- *Algorithm complexity lives in MLCore* — easy to test independently, natural fit/transform API, clean Armadillo interop.
- *Pipeline integration lives in TransformsV2* — thin wrappers (~30 lines each) that construct the MLCore operation, call fit_transform, and wrap the result as TensorData.
- *Tapkee dependency isolated to MLCore* — doesn't pollute TransformsV2 compile times.
- Respects existing project conventions for both systems.
- Full pipeline composability: `IntervalReduction → PCA → KMeans` as a JSON pipeline.
- Automatic UI generation via ParameterSchema for the TransformsV2 params.

**Cons:**
- Two files per algorithm (MLCore operation + TransformsV2 wrapper). Slightly more boilerplate than a monolithic approach, but each file is small and focused.

---

## Recommended Architecture (Option D) — Detailed Design

### 1. New Base Class: `MLDimReductionOperation`

```cpp
// src/MLCore/models/MLDimReductionOperation.hpp

class MLDimReductionOperation {
public:
    virtual ~MLDimReductionOperation() = default;

    /// @brief Get the algorithm name
    [[nodiscard]] virtual auto getName() const -> std::string = 0;

    /// @brief Fit model to data and return the transformed result
    /// @param features  (D × N) matrix — D features, N observations (mlpack convention)
    /// @param params    Algorithm-specific parameters
    /// @return (K × N) matrix — K output dimensions, N observations
    [[nodiscard]] virtual auto fitTransform(
        arma::mat const & features,
        MLModelParametersBase const * params) -> std::optional<arma::mat> = 0;

    /// @brief Project new data using a previously fitted model
    /// @pre isFitted() must be true
    [[nodiscard]] virtual auto transform(
        arma::mat const & features) -> std::optional<arma::mat> = 0;

    /// @brief Check if model has been fitted
    [[nodiscard]] virtual auto isFitted() const -> bool = 0;

    /// @brief Number of output dimensions after transformation
    [[nodiscard]] virtual auto outputDimensions() const -> std::size_t = 0;

    /// @brief Algorithm-specific metadata (explained variance, etc.)
    [[nodiscard]] virtual auto metadata() const
        -> std::unordered_map<std::string, double> { return {}; }
};
```

### 2. Example: PCA Operation

```cpp
// src/MLCore/models/unsupervised/PCAOperation.hpp

struct PCAParameters : MLModelParametersBase {
    std::size_t n_components = 2;
    bool scale = true;   // standardize features before PCA
};

class PCAOperation : public MLDimReductionOperation {
public:
    auto fitTransform(arma::mat const & features,
                      MLModelParametersBase const * params)
        -> std::optional<arma::mat> override;

    auto transform(arma::mat const & features)
        -> std::optional<arma::mat> override;

    auto isFitted() const -> bool override;
    auto outputDimensions() const -> std::size_t override;

    /// @brief Fraction of variance explained by each component
    auto explainedVarianceRatio() const -> std::vector<double>;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
```

### 3. TransformsV2 Container Transform Wrapper

```cpp
// src/TransformsV2/algorithms/TensorPCA/TensorPCA.hpp

struct TensorPCAParams {
    std::size_t n_components = 2;
    bool scale = true;
};

// src/TransformsV2/algorithms/TensorPCA/TensorPCA.cpp

auto tensorPCA(TensorData const & input,
               TensorPCAParams const & params,
               ComputeContext const & /*ctx*/)
    -> std::shared_ptr<TensorData>
{
    // 1. Convert TensorData → arma::mat (transpose to mlpack convention)
    auto const & mat = input.asArmadilloMatrix();
    arma::mat features = arma::conv_to<arma::mat>::from(mat.t());

    // 2. Run MLCore PCA
    PCAParameters mlparams;
    mlparams.n_components = params.n_components;
    mlparams.scale = params.scale;

    PCAOperation pca;
    auto result = pca.fitTransform(features, &mlparams);
    if (!result) return nullptr;

    // 3. Transpose back and wrap as TensorData
    arma::fmat output = arma::conv_to<arma::fmat>::from(result->t());
    std::vector<std::string> col_names;
    for (std::size_t i = 0; i < params.n_components; ++i)
        col_names.push_back("PC" + std::to_string(i + 1));

    return std::make_shared<TensorData>(
        TensorData::createFromArmadillo(std::move(output), std::move(col_names)));
}
```

Registration in `RegisteredTransforms.cpp`:
```cpp
void registerTensorPCA() {
    auto & registry = ElementRegistry::instance();
    registry.registerContainerTransform<TensorData, TensorData, TensorPCAParams>(
        "TensorPCA",
        tensorPCA,
        ContainerTransformMetadata{
            .description = "Principal Component Analysis for dimensionality reduction",
            .category = "Dimensionality Reduction",
            .input_type_name = "TensorData",
            .output_type_name = "TensorData",
            .params_type_name = "TensorPCAParams",
            .is_expensive = true,
            .is_deterministic = true,
            .supports_cancellation = false
        });
}
```

---

## Tapkee Integration Strategy

[Tapkee](https://github.com/lisitsyn/tapkee) is a header-only C++ library for dimensionality reduction. It provides:

| Algorithm | Tapkee Method | Notes |
|-----------|---------------|-------|
| **PCA** | `tapkee::PCA` | Linear, deterministic, fast |
| **t-SNE** | `tapkee::tDistributedStochasticNeighborEmbedding` | Non-linear, stochastic, O(N²) |
| **Isomap** | `tapkee::Isomap` | Non-linear, geodesic distances |
| **LLE** | `tapkee::LocallyLinearEmbedding` | Non-linear, preserves local structure |
| **Laplacian Eigenmaps** | `tapkee::LaplacianEigenmaps` | Spectral, graph-based |
| **MDS** | `tapkee::MultidimensionalScaling` | Classical scaling |
| **Diffusion Maps** | `tapkee::DiffusionMap` | Non-linear, diffusion distances |
| **Factor Analysis** | `tapkee::FactorAnalysis` | Linear, probabilistic |

### Dependency Management

**Option 1 (Recommended): vcpkg port**
```json
// vcpkg.json
{
    "dependencies": [
        { "name": "tapkee" }
    ]
}
```

**Option 2: FetchContent**
```cmake
# src/MLCore/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(tapkee
    GIT_REPOSITORY https://github.com/lisitsyn/tapkee.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(tapkee)
```

**Option 3: Header-only vendoring** — Copy tapkee headers into `external/tapkee/`. Simple but harder to update.

Tapkee uses Eigen internally. Since the project already uses Armadillo, a thin adapter for Armadillo ↔ Eigen conversion would be needed, or tapkee can be configured to use its dense matrix callbacks directly with raw data pointers.

### Alternative: mlpack's Built-In Dimensionality Reduction

mlpack (already a dependency) provides PCA and some related methods. For PCA specifically, using mlpack avoids adding a new dependency entirely:

```cpp
#include <mlpack/methods/pca/pca.hpp>

mlpack::PCA pca;
pca.Apply(features, n_components);  // in-place reduction
```

**Recommendation:** Start with mlpack's PCA (zero new dependencies), then add tapkee for t-SNE, UMAP, and manifold methods that mlpack doesn't provide.

---

## Shape Validation Strategy

Tensor transforms are shape-dependent. The system needs a way to validate that the input tensor has the correct dimensionality and minimum size.

### Compile-Time: Template Tags

```cpp
// Concept for shape constraints
template<typename T>
concept Tensor2DLike = requires(T const & t) {
    { t.numDimensions() } -> std::same_as<std::size_t>;
    requires (T::DataTraits::is_ragged == false);
};
```

### Runtime: Precondition Checks in Container Transform

```cpp
auto tensorPCA(TensorData const & input, TensorPCAParams const & params,
               ComputeContext const & ctx) -> std::shared_ptr<TensorData>
{
    if (input.numDimensions() != 2) {
        spdlog::error("TensorPCA requires 2D input, got {}D", input.numDimensions());
        return nullptr;
    }
    if (input.shape()[0] < params.n_components) {
        spdlog::error("TensorPCA: n_components ({}) exceeds number of rows ({})",
                      params.n_components, input.shape()[0]);
        return nullptr;
    }
    // ... proceed
}
```

### ParameterUIHints for Dynamic Constraints

```cpp
template<>
struct ParameterUIHints<TensorPCAParams> {
    static auto hints() -> std::vector<FieldHint> {
        return {
            {"n_components", {.min = 1, .tooltip = "Number of principal components to retain"}}
        };
    }
};
```

---

## Implementation Roadmap

### Phase 1: Foundation — PCA via mlpack + Row Time Preservation ✅
**Goal:** End-to-end TensorData → TensorData working through TransformsV2 pipeline, with time indices preserved for scatter plot integration.

1. ✅ **Add `MLDimReductionOperation` base class to MLCore**
   - New file: `src/MLCore/models/MLDimReductionOperation.hpp`
   - Pure virtual interface: `fitTransform()`, `transform()`, `isFitted()`, `outputDimensions()`

2. ✅ **Implement `PCAOperation` in MLCore using mlpack**
   - New files: `src/MLCore/models/unsupervised/PCAOperation.hpp/.cpp`
   - Use `mlpack::PCA` internally
   - Store eigenvectors for re-projection via `transform()`
   - Expose `explainedVarianceRatio()`

3. ✅ **Create TransformsV2 `TensorPCA` wrapper**
   - New files: `src/TransformsV2/algorithms/TensorPCA/TensorPCA.hpp/.cpp`
   - reflect-cpp `TensorPCAParams` struct
   - Container transform: `TensorData → TensorData`
   - Register in `RegisteredTransforms.cpp`
   - **Critical: Preserve `RowDescriptor` from input.** If input has `TimeFrameIndex` rows, output must copy them. This enables scatter plot time-based joining and point → frame navigation.

   ```cpp
   // Preserve row descriptor from input
   auto row_desc = input.rowDescriptor();           // Copy RowDescriptor
   auto time_frame = input.timeFrame();             // Copy shared TimeFrame
   // ... run PCA ...
   // Build output with same row semantics:
   auto result = TensorData::createTimeSeries2D(
       flat_output, num_rows, n_components,
       row_desc.timeIndexStorage(),   // Same time indices as input
       time_frame,                     // Same TimeFrame
       col_names);                     // "PC1", "PC2", ...
   ```

4. ✅ **Add unit tests**
   - MLCore PCA: correctness on known matrix (identity covariance, etc.)
   - TransformsV2 integration: round-trip through registry
   - Shape validation: reject non-2D, too-few-rows, etc.
   - **Row descriptor preservation:** Verify output TimeFrameIndex matches input

5. **Verify pipeline composability** (manual verification)
   - JSON pipeline: `IntervalReduction → TensorPCA → KMeans`

6. **Verify scatter plot integration (manual test)**
   - Load video → run encoder → apply PCA via DataTransform_Widget
   - Open ScatterPlotWidget → select PCA tensor → set X=PC1, Y=PC2
   - Confirm points render, double-click navigates to frame

### Phase 2: Scatter Selection → Entity Groups ✅
**Goal:** Users can lasso points in the scatter plot and create entity groups for downstream analysis.

This phase bridges the **visualization → interaction** gap. The scatter widget already has polygon selection and `GroupManager` integration for *reading*. This phase adds *writing*.

1. ✅ **"Create Group from Selection" context menu action in `ScatterPlotOpenGLWidget`**
   - Already implemented via `GroupContextMenuHandler` (shared with MediaWidget, LinePlotOpenGLWidget, etc.)
   - `createContextMenu()` wires up callbacks: `getSelectedEntities`, `clearSelection`, `hasSelection`, `onGroupOperationCompleted`
   - Observer notification automatically triggers scatter recolor via `applyGroupColorsToScene()`

2. ✅ **"Add to Existing Group" sub-menu**
   - Provided by `GroupContextMenuHandler::setupGroupMenuSection()` — lists existing groups from `EntityGroupManager`

3. ✅ **`EntityId` resolution for TensorData rows**
   - **Decision:** Option B — only TimeFrameIndex and Interval rows support entity selection. Ordinal rows return `nullopt`.
   - **Fix:** `getEntityIdForPoint()` was fabricating `EntityId{static_cast<uint64_t>(tfi.getValue())}` which didn't go through `EntityRegistry`. Fixed to use `DataManager::ensureTimeEntityId(time_key, tfi)` via `DataManager::getTimeKey(source_data_key)`.
   - Added `source_data_key` and `source_row_type` fields to `ScatterPointData` to carry source provenance from `buildScatterPoints()`.
   - **Future extension:** Once interval-row support for non-analog types lands (item #1 in `tensor_roadmap.md`), the same entity preservation logic should generalize to interval-row TensorData.

4. ✅ **Reuse existing `TimeEntity` EntityKind for TensorData-sourced entities**
   - `DataManager::ensureTimeEntityId(TimeKey, TimeFrameIndex)` already creates `TimeEntity` kind entities via `EntityRegistry::ensureId()`.
   - No new `EntityKind` needed — TimeEntity covers the encoder workflow correctly.

5. ✅ **Selection → group creation → scatter recolor loop**
   - All 23 related tests pass (scatter_plot_widget, TransformsV2, MLCore, TimeEntity, EntityGroupManager)
   - Manual testing recommended for full end-to-end visual verification

### Phase 3: Tapkee Integration (t-SNE, Manifold Methods)
**Goal:** Non-linear dimensionality reduction via tapkee.

1. **Add tapkee dependency** (vcpkg or FetchContent)

2. **Implement tapkee adapter in MLCore**
   - Armadillo ↔ Eigen data bridge (or raw pointer callbacks)
   - `TSNEOperation`, `IsomapOperation`, `LaplacianEigenmapsOperation`

3. **Register corresponding TransformsV2 wrappers**
   - `TensorTSNE`, `TensorIsomap`, `TensorLaplacianEigenmaps`
   - Each with reflect-cpp parameter structs
   - All must preserve `RowDescriptor` (same as PCA wrapper)

4. **Add `DimensionalityReductionPipeline` to MLCore** (analogous to `ClusteringPipeline`)
   - Stages: ValidateFeatures → ConvertFeatures → FitTransform → WriteOutput
   - Handles time alignment, NaN dropping, optional normalization

### Phase 4: MLCore Widget Dim Reduction Tab + Auto-Focus
**Goal:** Integrated UI for dimensionality reduction with scatter visualization feedback loop.

1. **Add "Dimensionality Reduction" tab to MLCore_Widget**
   - Follows existing tab pattern (Classification, Clustering)
   - Sub-panels:
     - `FeatureSelectionPanel` (reuse existing — selects input TensorData)
     - `DimReductionAlgorithmPanel` (algorithm combo: PCA, t-SNE, ...; param widgets via AutoParamWidget)
     - `OutputPreviewPanel` (shows output shape, explained variance for PCA)
   - "Run" button triggers `DimensionalityReductionPipeline`
   - Output written as new TensorData key (e.g., `"encoder_features_pca"`)

2. **Auto-focus output after transform execution**
   - After DataTransform_Widget or MLCore_Widget writes a result:
     ```cpp
     _selection_context->setDataFocus(output_key, "TensorData", source);
     ```
   - All `DataFocusAware` widgets update:
     - ScatterPlotPropertiesWidget → refreshes key combos, output key appears at top
     - TensorInspector → loads new tensor
     - DataTransform_Widget → updates valid transforms list

3. **"Visualize in Scatter" quick action**
   - This is implemented as a **ContextAction** registered by ScatterPlotWidget (see [Inter-Widget Communication Roadmap — Proposal 1](inter_widget_communication_roadmap.md#proposal-1-context-aware-actions-priority-high)).
   - The ContextAction's `is_applicable` returns true when a TensorData with ≥2 columns is focused.
   - Its `execute()` body creates or finds a ScatterPlotWidget via `EditorRegistry` and sets X/Y from the first two columns.
   - Additionally, MLCore_Widget output panel shows a "↗ Scatter Plot" button as a direct shortcut.
   - This avoids manual axis configuration for the common 2-component case.

### Phase 5: Clustering ↔ Scatter Feedback Loop
**Goal:** Tight integration between MLCore clustering and scatter visualization.

> **Depends on:** [Inter-Widget Communication Roadmap — Proposal 1 (Context-Aware Actions)](inter_widget_communication_roadmap.md#proposal-1-context-aware-actions-priority-high). The context menu actions below are implemented as **ContextActions** registered by MLCore_Widget and ScatterPlotWidget respectively.

1. **Run clustering on reduced features directly from scatter context menu**
   - Implemented as a ContextAction (e.g., `"mlcore.cluster_tensor"`) registered by MLCore_Widget
   - `is_applicable`: focused data is TensorData
   - `execute()`: opens/focuses MLCore_Widget, sets feature tensor key, activates Clustering tab
   - Right-click scatter → "Cluster Points..." → MLCore_Widget opens with correct tensor pre-selected
   - Output: entity groups written to `EntityGroupManager`
   - Scatter auto-recolors via existing group generation counter mechanism

2. **Selective clustering: cluster only selected points**
   - User selects a subset in scatter → right-click → "Cluster Selection..."
   - Creates a filtered TensorData view (or copy) containing only selected rows
   - Runs clustering on the subset
   - Maps cluster assignments back to full-dataset entity IDs
   - Result: sub-groups within the original scatter

3. **Cluster label overlay in scatter**
   - Display cluster centroid labels (e.g., "Cluster 0 (n=42)") as text annotations
   - Toggleable via scatter properties panel

### Phase 6: Elemental Tensor Transforms
**Goal:** Support simple per-element operations on tensor data.

1. **Register element-level transforms for TensorData**
   - These would operate on individual `float` values or rows
   - Examples: `TensorSquare` (square all values), `TensorLog`, `TensorClamp`
   - Since TensorData stores `float`, the element type is essentially `std::span<float>` (a row) or a single `float`

2. **Consider ContainerTraits for TensorData**
   - Map `std::span<float const>` (row view) ↔ TensorData for row-level element transforms
   - This would enable the lifting mechanism: define `f(row) → row`, auto-apply to all rows

### Phase 7: Advanced Features
**Goal:** Production-quality tooling.

1. **Fit/project workflow**
   - `TensorPCAFit` container transform that stores the model in `PipelineValueStore`
   - `TensorPCAProject` that retrieves the model and projects new data
   - Enables train/test workflows

2. **Explained variance widget**
   - Scree plot showing variance explained per component
   - Interactive component count selection

3. **3D embedding visualization**
   - 3D scatter plot (OpenGL) for 3-component embeddings
   - Rotation, zoom, color-coded by group

4. **Benchmarks**
   - Google Benchmark suite for PCA/t-SNE at various matrix sizes
   - Regression tracking for large-dataset performance

---

## Dependency Graph

```
                    ┌────────────────────────────┐
                    │     ScatterPlotWidget       │
                    │  (visualization + selection) │
                    └──────────┬─────────────────┘
                               │ reads TensorData, writes EntityGroups
                               ▼
┌─────────────────┐     ┌──────────────┐     ┌───────────────────┐
│   TransformsV2  │────▶│   MLCore     │     │ EntityGroupManager│
│                 │     │              │     │                   │
│ TensorPCA       │     │ PCAOperation │     │ createGroup()     │
│ TensorTSNE      │     │ TSNEOperation│     │ addEntities()     │
│ (thin wrappers) │     │ Clustering   │────▶│ notifyObservers() │
└────────┬────────┘     └──────┬───────┘     └────────┬──────────┘
         │                     │                      │
         ▼                     ▼                      ▼
┌─────────────────┐     ┌──────────────┐     ┌───────────────────┐
│   TensorData    │     │   Tapkee     │     │ SelectionContext   │
│   (DataObjects) │     │ (header-only)│     │ (broadcasts to    │
└─────────────────┘     └──────────────┘     │  all widgets)     │
                                             └───────────────────┘
```

**Widget interaction is mediated entirely through:**
1. **DataManager** — transforms write output tensors; widgets observe additions
2. **SelectionContext** — `setDataFocus()` broadcasts which data key is active; `DataFocusAware` widgets auto-update
3. **EntityGroupManager** — scatter selection creates groups; group observers notify all widgets (including scatter recolor)

No widget holds a pointer to any other widget.

---

## Cross-Widget Communication Patterns (Reference)

### Pattern 1: Transform Output → Scatter Visualization

```
DataTransform_Widget
  │ executes TensorPCA → DataManager::setData("features_pca", tensor)
  │ calls SelectionContext::setDataFocus("features_pca", "TensorData", source)
  ▼
ScatterPlotPropertiesWidget (DataFocusAware)
  │ onDataFocusChanged("features_pca") → refreshes key combos
  │ User selects X=PC1, Y=PC2 (or auto-configured)
  ▼
ScatterPlotOpenGLWidget
  │ rebuildScene() using TensorData columns
  ▼
Rendered scatter plot with TimeFrameIndex-based points
```

### Pattern 2: Scatter Selection → Entity Group

```
ScatterPlotOpenGLWidget
  │ User polygon-selects points → selected_indices populated
  │ Right-click → "Create Group from Selection..."
  │
  │ selected_indices[i] → ScatterPointData::time_indices[i]
  │ → EntityRegistry::entityIdFor(data_key, time_index)
  │ → EntityGroupManager::createGroup("cluster_manual")
  │ → EntityGroupManager::addEntitiesToGroup(gid, entity_ids)
  ▼
EntityGroupManager notifies observers
  ├→ ScatterPlotWidget: group generation changed → scene dirty → recolor
  ├→ GroupManagementWidget: refreshes group list
  └→ MLCore_Widget: new group available for labeling
```

### Pattern 3: Clustering → Scatter Color Update

```
MLCore_Widget
  │ User selects "features_pca" tensor, runs K-Means (k=3)
  │ ClusteringPipeline writes entity groups → EntityGroupManager
  ▼
EntityGroupManager notifies observers
  ├→ ScatterPlotWidget: detects generation counter change
  │   → marks scene dirty → applyGroupColorsToScene()
  │   → points colored by cluster membership
  └→ GroupManagementWidget: shows "Cluster_0", "Cluster_1", "Cluster_2"
```

---

## File Inventory

### Phase 1 (PCA Foundation)

| File | Action | Purpose |
|------|--------|---------|
| `src/MLCore/models/MLDimReductionOperation.hpp` | Create | Base class for dim reduction |
| `src/MLCore/models/unsupervised/PCAOperation.hpp` | Create | PCA interface |
| `src/MLCore/models/unsupervised/PCAOperation.cpp` | Create | PCA implementation (mlpack) |
| `src/MLCore/CMakeLists.txt` | Edit | Add new source files |
| `src/TransformsV2/algorithms/TensorPCA/TensorPCA.hpp` | Create | Params struct |
| `src/TransformsV2/algorithms/TensorPCA/TensorPCA.cpp` | Create | Container wrapper (preserves RowDescriptor) |
| `src/TransformsV2/core/RegisteredTransforms.cpp` | Edit | Add registration call |
| `src/TransformsV2/CMakeLists.txt` | Edit | Add new algorithm files |
| `tests/MLCore/test_PCAOperation.cpp` | Create | Unit tests |
| `tests/TransformsV2/test_TensorPCA.cpp` | Create | Integration tests (including row preservation) |
| `docs/developer/MLCore/dim_reduction.qmd` | Create | Developer docs |
| `docs/developer/transforms_v2/tensor_transforms.qmd` | Create | Developer docs |

### Phase 2 (Scatter Selection → Groups) ✅

| File | Action | Purpose |
|------|--------|---------|
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Core/ScatterPointData.hpp` | Edit | Added `source_data_key` and `source_row_type` for entity resolution |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Core/BuildScatterPoints.cpp` | Edit | Propagate source context to ScatterPointData |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Rendering/ScatterPlotOpenGLWidget.cpp` | Edit | Fixed `getEntityIdForPoint()` to use `ensureTimeEntityId()` |

### Phase 3 (Tapkee)

| File | Action | Purpose |
|------|--------|---------|
| `vcpkg.json` or `src/MLCore/CMakeLists.txt` | Edit | Add tapkee dependency |
| `src/MLCore/models/unsupervised/TSNEOperation.hpp/.cpp` | Create | t-SNE via tapkee |
| `src/MLCore/models/unsupervised/IsomapOperation.hpp/.cpp` | Create | Isomap via tapkee |
| `src/TransformsV2/algorithms/TensorTSNE/TensorTSNE.hpp/.cpp` | Create | TransformsV2 wrapper |
| `src/TransformsV2/algorithms/TensorIsomap/TensorIsomap.hpp/.cpp` | Create | TransformsV2 wrapper |

### Phase 4 (MLCore Dim Reduction Tab)

| File | Action | Purpose |
|------|--------|---------|
| `src/WhiskerToolbox/MLCore_Widget/UI/DimReductionPanel/` | Create | New tab panel(s) |
| `src/WhiskerToolbox/MLCore_Widget/MLCoreWidget.hpp/.cpp` | Edit | Add tab, auto-focus output |
| `src/WhiskerToolbox/DataTransform_Widget/DataTransform_Widget.cpp` | Edit | Auto-focus output after execution |
## Priority Summary

| # | Phase | Priority | Complexity | Key Deliverable |
|---|-------|----------|------------|-----------------|
| 1 | PCA Foundation | **High** | Medium | TensorData → TensorData through TransformsV2; row preservation |
| 2 | Scatter Selection → Groups | **High** | Low-Medium | "Create Group from Selection" context menu in scatter |
| 3 | Tapkee (t-SNE, manifold) | Medium | Medium | Non-linear dim reduction algorithms |
| 4 | MLCore Dim Reduction Tab | Medium | Medium | Integrated UI; auto-focus output |
| 5 | Clustering ↔ Scatter Loop | Medium | Low | Context-menu clustering; sub-selection clustering |
| 6 | Elemental Tensor Transforms | Low | Low | Row-level `f(row) → row` element transforms |
| 7 | Advanced Features | Low | Varies | Fit/project, scree plot, 3D scatter, benchmarks |

Phases 1-2 together deliver the complete end-to-end workflow: encoder → PCA → scatter → select cluster → group. Phases 3-5 extend it with better algorithms and tighter UI integration. Phases 6-7 are quality-of-life improvements.

---

## Decision Summary

| Question | Recommendation | Rationale |
|----------|---------------|-----------|
| Where do algorithms live? | **MLCore** | Natural fit/transform API; isolates tapkee dependency; testable independently |
| Where does pipeline integration live? | **TransformsV2** (container transforms) | Full pipeline composability; automatic UI; consistent with existing patterns |
| Initial backend? | **mlpack PCA** (already a dependency) | Zero new dependencies for Phase 1 |
| Manifold methods backend? | **Tapkee** (Phase 3) | Best C++ manifold learning library; header-only; Eigen-based |
| Cross-widget communication? | **SelectionContext + EntityGroupManager + DataManager observers** | All infrastructure exists; no widget-to-widget pointers; loose coupling via broadcasts |
| How do scatter selections become groups? | **Context menu → EntityGroupManager** | Scatter already has selection + GroupManager read access; add write path |
| How does clustering feedback to scatter? | **EntityGroupManager observer → generation counter → scene dirty** | Already implemented in scatter widget; no new wiring needed |
| Elemental tensor transforms? | **Phase 6** (row-level element transforms) | Nice-to-have; requires ContainerTraits work for TensorData |
| Separate `TensorTransforms` library? | **No** | Hybrid approach avoids new infrastructure while keeping concerns separated |

# Tensor Transform Library — Design Roadmap

**Last updated:** 2026-03-29

## Motivation

TensorData is the richest numeric container in WhiskerToolbox, supporting 2D matrices, 3D cubes, and N-dimensional arrays across multiple storage backends (Armadillo, Dense, LibTorch). Many scientific workflows require **tensor-in → tensor-out** operations: dimensionality reduction (PCA, t-SNE, UMAP), spectral decomposition (SVD, NMF), normalization, slicing, and reshaping. These transforms differ from element-level transforms because:

1. **Shape-dependence** — A PCA requires 2D; a conv-filter might need 3D.
2. **Whole-container semantics** — Operates on the *entire* matrix, not element-by-element.
3. **Output shape differs from input** — PCA(N×D) → (N×K) where K < D.
4. **Shared state** — Some algorithms produce fitted models reusable for projection.

---

## Target Workflow

The primary motivating use case is the **encoder → embedding → visualization → interaction** loop:

```
Video (frames)
  │  DeepLearning_Widget: encoder + global average pooling
  ▼
TensorData  (N×192, TimeFrameIndex rows, "encoder_features")
  │  DataTransform_Widget or MLCore Dim Reduction tab
  ▼
TensorData  (N×2, TimeFrameIndex rows, columns "PC1"/"PC2")
  │  ScatterPlotWidget: X=PC1, Y=PC2, color-by-group
  ▼
Scatter visualization with interactive selection
  │  User selects cluster → entity group created
  ▼
EntityGroupManager → downstream analysis
```

### Workflow Status

| Step | Component | Status |
|------|-----------|--------|
| Encode video → TensorData | DeepLearning_Widget + ResultProcessor | ✅ Exists |
| PCA on TensorData | MLCore PCAOperation + TensorPCA TransformsV2 wrapper | ✅ Phase 1 complete |
| t-SNE / manifold methods | tapkee integration | Phase 3 (t-SNE complete, others pending) |
| Scatter plot of reduced features | ScatterPlotWidget (TensorData columns) | ✅ Exists |
| Color points by group | ScatterPlotWidget (`colorByGroup` + GroupManager) | ✅ Exists |
| Select points → create group | ScatterPlotWidget → EntityGroupManager | ✅ Phase 2 complete |
| Cluster automatically | MLCore ClusteringPipeline + ContextAction | ✅ Phase 5 complete |
| Propagate groups to other widgets | SelectionContext + DataFocusAware | ✅ Exists |
| Dim reduction UI | MLCore_Widget Dim Reduction tab | ✅ Phase 4 complete |
| Scatter ↔ clustering loop | ContextActions + selective clustering + label overlay | ✅ Phase 5 complete |
| Supervised dim reduction (logit projection) | MLCore LogitProjectionOp + SupervisedDimReductionPipeline | Phase 8 (8a complete) |

---

## Architecture: Hybrid (Option D)

**Decision:** Algorithms live in MLCore; pipeline integration lives in TransformsV2. This was chosen over three alternatives (pure TransformsV2, standalone TensorTransforms library, pure MLCore). See the Decision Summary at the end of this document for rationale.

```
MLCore (algorithms)                TransformsV2 (pipeline wrappers)
├── MLDimReductionOperation        ├── TensorPCA (container transform)
├── MLSupervisedDimRedOp (P8)      ├── TensorTSNE (Phase 3)
├── PCAOperation (mlpack)          ├── TensorIsomap (Phase 3)
├── TSNEOperation (Phase 3)        ├── TensorLogitProjection (P8)
├── LogitProjectionOperation (P8)  │
├── SoftmaxRegressionOp ✅         │
├── DimReductionPipeline           │
├── SupervisedDimRedPipeline (P8)  │
└── ClusteringPipeline             Widget Communication
                                   ├── SelectionContext (setDataFocus)
                                   ├── ContextActions (2 registered)
                                   └── EntityGroupManager (observers)
```

---

## Implementation Roadmap

### Phase 1: PCA Foundation ✅

**Delivered:** End-to-end TensorData → TensorData through TransformsV2 pipeline with time indices preserved.

- `MLDimReductionOperation` base class (`src/MLCore/models/MLDimReductionOperation.hpp`)
- `PCAOperation` using mlpack (`src/MLCore/models/unsupervised/PCAOperation.hpp/.cpp`)
- `TensorPCA` TransformsV2 container transform with `RowDescriptor` preservation
- Unit tests: PCA correctness, TransformsV2 integration, shape validation, row descriptor preservation

**Critical design decision:** Container transforms preserve `RowDescriptor` from input — if input has `TimeFrameIndex` rows, output must copy them. This enables scatter plot time-based joining and point → frame navigation.

### Phase 2: Scatter Selection → Entity Groups ✅

**Delivered:** Users can lasso points in scatter and create entity groups.

- "Create Group from Selection" and "Add to Existing Group" context menus via `GroupContextMenuHandler`
- `EntityId` resolution: only `TimeFrameIndex` and `Interval` rows support entity selection (Ordinal rows return `nullopt`). Uses `DataManager::ensureTimeEntityId()` via `EntityRegistry`.
- Added `source_data_key` and `source_row_type` to `ScatterPointData` for provenance tracking
- Selection → group creation → scatter recolor loop verified (23 tests pass)

**Future extension:** Once interval-row support for non-analog types lands, entity preservation should generalize to interval-row TensorData.

### Phase 3: Tapkee Integration — IN PROGRESS

**Goal:** Non-linear dimensionality reduction (t-SNE, manifold methods).

**t-SNE: Complete.** Tapkee integrated via FetchContent (header-only, own CMakeLists.txt bypassed). `TSNEOperation` in MLCore wraps tapkee's `tDistributedStochasticNeighborEmbedding`. `TensorTSNE` registered as TransformsV2 container transform. Full test coverage.

**Remaining:** Isomap, Laplacian Eigenmaps, and other manifold methods.

**Note:** `DimReductionPipeline` already exists from Phase 4 — only tapkee-specific operations and TransformsV2 wrappers need to be added for the remaining methods.

**Steps:**

1. **Add tapkee dependency** — vcpkg preferred, FetchContent as fallback. Tapkee is header-only (Eigen internally); needs Armadillo ↔ Eigen adapter or raw pointer callbacks.

2. **Implement tapkee operations in MLCore:**
   - `TSNEOperation` — `MLDimReductionOperation` subclass
   - `IsomapOperation` — geodesic distance-based manifold learning
   - `LaplacianEigenmapsOperation` — spectral, graph-based

3. **Register TransformsV2 wrappers:**
   - `TensorTSNE`, `TensorIsomap`, `TensorLaplacianEigenmaps`
   - Each with reflect-cpp parameter structs + `RowDescriptor` preservation

4. **Update DimReductionPanel UI:**
   - Add algorithm selection dropdown (currently PCA only)
   - Algorithm-specific parameter forms via `ParameterSchema`

**Alternative for PCA:** mlpack's built-in `mlpack::PCA` is already used. For methods mlpack doesn't provide (t-SNE, Isomap, LLE, Diffusion Maps), tapkee fills the gap.

| Algorithm | Tapkee Method | Notes |
|-----------|---------------|-------|
| t-SNE | `tapkee::tDistributedStochasticNeighborEmbedding` | Non-linear, stochastic, O(N²) |
| Isomap | `tapkee::Isomap` | Non-linear, geodesic distances |
| LLE | `tapkee::LocallyLinearEmbedding` | Non-linear, preserves local structure |
| Laplacian Eigenmaps | `tapkee::LaplacianEigenmaps` | Spectral, graph-based |
| MDS | `tapkee::MultidimensionalScaling` | Classical scaling |
| Diffusion Maps | `tapkee::DiffusionMap` | Non-linear, diffusion distances |

**Files to create/edit:**

| File | Action |
|------|--------|
| `vcpkg.json` or `src/MLCore/CMakeLists.txt` | Add tapkee dependency |
| `src/MLCore/models/unsupervised/TSNEOperation.hpp/.cpp` | Create: t-SNE via tapkee |
| `src/MLCore/models/unsupervised/IsomapOperation.hpp/.cpp` | Create: Isomap via tapkee |
| `src/TransformsV2/algorithms/TensorTSNE/TensorTSNE.hpp/.cpp` | Create: TransformsV2 wrapper |
| `src/TransformsV2/algorithms/TensorIsomap/TensorIsomap.hpp/.cpp` | Create: TransformsV2 wrapper |
| `tests/MLCore/test_TSNEOperation.cpp` | Create: unit tests |

### Phase 4: MLCore Widget Dim Reduction Tab + Auto-Focus ✅

**Delivered:** Integrated dim reduction UI with scatter visualization feedback loop.

- "Dimensionality Reduction" tab in MLCore_Widget with `DimReductionPanel` (tensor selection, PCA config, n_components, scale, z-score)
- `DimReductionPipeline`: ValidateFeatures → ConvertFeatures → FitTransform → WriteOutput
- Worker thread (`DimReductionPipelineWorker`) with deferred main-thread writes for thread safety
- Shows explained variance ratio per component after completion
- Auto-focus output: both `MLCoreWidget` and `DataTransform_Widget` call `SelectionContext::setDataFocus()` after transform
- `scatter_plot.visualize_2d_tensor` ContextAction enables one-click scatter visualization

### Phase 5: Clustering ↔ Scatter Feedback Loop ✅

**Delivered:** Tight integration between MLCore clustering and scatter visualization.

- `mlcore.cluster_tensor` ContextAction with `setActiveTab(1)` to switch to Clustering tab
- Scatter context menu dynamically populates applicable ContextActions from `SelectionContext`
- **Selective clustering:** "Cluster Selection..." creates filtered TensorData copy from polygon-selected rows, stores as `"<key>_selection"`, triggers clustering ContextAction
- **Cluster label overlay:** QPainter overlay draws "GroupName (n=count)" at cluster centroids with group color + dark outline. Toggled via `show_cluster_labels` checkbox (serialized in `ScatterPlotStateData`).

### Phase 6: Elemental Tensor Transforms

**Goal:** Per-element operations on tensor data (square, log, clamp).

1. Register element-level transforms for TensorData (row-level `f(row) → row`)
2. Consider `ContainerTraits` for TensorData to enable automatic lifting

### Phase 7: Advanced Features

1. **Fit/project workflow** — `TensorPCAFit` + `TensorPCAProject` using `PipelineValueStore`
2. **Explained variance widget** — Scree plot with interactive component count selection
3. **3D embedding visualization** — OpenGL 3D scatter for 3-component embeddings
4. **Benchmarks** — Google Benchmark suite for PCA/t-SNE at various matrix sizes

### Phase 8: Supervised Dimensionality Reduction (Logit Projection) — IN PROGRESS (8a complete)

**Goal:** Use logistic / softmax regression as a supervised dimensionality reduction method. Given a high-dimensional TensorData (N×D) and class labels, the model learns a linear projection that maximally separates the classes. The output is a **logits TensorData** (N×C) where C is the number of classes — a C-dimensional space optimized for class discrimination.

**Mathematical basis:** Logistic regression learns a weight matrix $W \in \mathbb{R}^{C \times D}$ and bias $b$. The logits $z = Wx + b$ form a C-dimensional linear subspace that is optimally discriminative under the cross-entropy loss. This is closely related to Fisher's Linear Discriminant Analysis (LDA) and can be viewed as a supervised counterpart to PCA.

**Key differences from unsupervised dim reduction:**
- Requires **class labels** in addition to features → must integrate `LabelAssembler`
- Output dimensionality is **determined by the number of classes**, not a user parameter
- Uses the existing `LogisticRegressionOperation` (binary) and new `SoftmaxRegressionOperation` (multi-class) as training backends
- Extracts raw **logits** (pre-sigmoid/softmax activations) rather than predictions or probabilities

**Primary use case:** The encoder → logit projection → scatter plot workflow:
```
TensorData  (N×192, "encoder_features")
  │  Supervised dim reduction: labels from DigitalIntervalSeries / EntityGroups
  ▼
TensorData  (N×C, logit columns "Logit:ClassName1" / "Logit:ClassName2" / ...)
  │  ScatterPlotWidget: X=Logit:Class0, Y=Logit:Class1, color-by-group
  ▼
Scatter visualization showing class separation in logit space
```

#### Phase 8a: SoftmaxRegressionOperation (MLCore algorithm) ✅

**Delivered:** Multi-class logistic regression via `mlpack::SoftmaxRegression<arma::mat>`, registered as `"Softmax Regression"` with task type `MultiClassClassification`. 11 test cases, 434 assertions.

- `SoftmaxRegressionOperation` — `MLModelOperation` subclass with pimpl, L-BFGS optimizer, cereal serialization. See `src/MLCore/models/supervised/SoftmaxRegressionOperation.hpp/.cpp`.
- `SoftmaxRegressionParameters` — `lambda` (0.0001), `max_iterations` (10000). Added to `src/MLCore/models/MLModelParameters.hpp`.
- Registered in `src/MLCore/models/MLModelRegistry.cpp`.
- Tests in `tests/MLCore/SoftmaxRegressionOperation.test.cpp` cover: metadata, 3/4/5-class training, binary training, error handling, prediction accuracy, probability output (sum-to-one, range), save/load round-trip, parameter variations, registry integration.
- Developer documentation: `docs/developer/MLCore/softmax_regression.qmd`.

**Implementation note:** `mlpack::SoftmaxRegression` requires template argument `<arma::mat>`. The constructor argument order is `(data, labels, numClasses, optimizer, lambda, fitIntercept)` — optimizer precedes lambda.

#### Phase 8b: Supervised Dim Reduction Operation (MLCore interface + algorithm)

**Goal:** Abstract base class for supervised dimensionality reduction, plus `LogitProjectionOperation` concrete implementation.

**Steps:**

1. **Create `MLSupervisedDimReductionOperation`** — new abstract base class extending `MLModelOperation`
   ```cpp
   class MLSupervisedDimReductionOperation : public MLModelOperation {
   public:
       /// Fit on labeled data and project to reduced space
       /// @param features  (D × N) input features in mlpack convention
       /// @param labels    (N,) class label per observation
       /// @param params    Algorithm-specific parameters
       /// @param result    Output: (C × N) reduced representation
       virtual bool fitTransform(
           arma::mat const & features,
           arma::Row<std::size_t> const & labels,
           MLModelParametersBase const * params,
           arma::mat & result) = 0;

       /// Project new unlabeled data using the fitted model
       virtual bool transform(
           arma::mat const & features,
           arma::mat & result) = 0;

       /// Number of output dimensions (= num_classes for logit projection)
       virtual std::size_t outputDimensions() const = 0;

       /// Class names corresponding to each output column
       virtual std::vector<std::string> outputColumnNames() const = 0;
   };
   ```

2. **Create `LogitProjectionOperation`** — concrete supervised dim reduction
   - Internally trains `LogisticRegressionOperation` (if 2 classes) or `SoftmaxRegressionOperation` (if C≥2)
   - Extracts model parameter matrix: `W = model.Parameters()` from mlpack
     - Binary: weight vector $w$ (D,) + bias scalar → logit $z = w^T x + b$ → 1D output (or 2D: [z, -z])
     - Softmax: weight matrix $W$ (C × (D+1)) → logits $Z = W_{[:,:D]} \cdot X + W_{[:,D]}$ → C-D output
   - `fitTransform()`: train then compute logits
   - `transform()`: project new data using stored $W$, $b$
   - `outputColumnNames()`: returns `["Logit:ClassName0", "Logit:ClassName1", ...]`

3. **Create `LogitProjectionParameters`** — parameter struct
   - `double lambda` — L2 regularization (default 0.0001)
   - `int max_iterations` — L-BFGS iterations (default 10000)
   - `bool scale_features` — optional feature standardization before fitting (default false)

4. **Register in `MLModelRegistry`** — new task type `SupervisedDimensionalityReduction` in `MLTaskType` enum, or reuse `DimensionalityReduction` with supervised handling

5. **Unit tests** — logit shape correctness, class separation quality, transform consistency, binary/multi-class paths

**Design decision: `MLTaskType` extension**

Add `SupervisedDimensionalityReduction` to the `MLTaskType` enum. This keeps supervised and unsupervised dim reduction discoverable via separate registry queries, which matters for the `DimReductionPanel` UI (Phase 8d) to show the correct algorithm list per mode.

**Files to create/edit:**

| File | Action |
|------|--------|
| `src/MLCore/models/MLSupervisedDimReductionOperation.hpp` | Create: abstract base |
| `src/MLCore/models/supervised/LogitProjectionOperation.hpp` | Create |
| `src/MLCore/models/supervised/LogitProjectionOperation.cpp` | Create |
| `src/MLCore/models/MLModelParameters.hpp` | Add `LogitProjectionParameters` |
| `src/MLCore/models/MLTaskType.hpp` | Add `SupervisedDimensionalityReduction` |
| `src/MLCore/models/MLModelRegistry.cpp` | Register `"Logit Projection"` |
| `tests/MLCore/test_LogitProjectionOperation.cpp` | Create |

#### Phase 8c: Supervised Dim Reduction Pipeline

**Goal:** End-to-end pipeline combining label assembly with dim reduction TensorData output.

This merges `DimReductionPipeline`'s TensorData output workflow with `ClassificationPipeline`'s label assembly stage from `LabelAssembler`.

**Steps:**

1. **Create `SupervisedDimReductionPipelineConfig`** — pipeline configuration
   ```cpp
   struct SupervisedDimReductionPipelineConfig {
       // -- Model --
       std::string model_name;                      // "Logit Projection"
       MLModelParametersBase const * model_params;  // LogitProjectionParameters

       // -- Features --
       std::string feature_tensor_key;              // input TensorData DM key

       // -- Labels --
       LabelAssemblyConfig label_config;            // same 4-mode variant from ClassificationPipeline
       std::string label_interval_key;              // DM key for DigitalIntervalSeries (if LabelFromIntervals)
       std::string label_event_key;                 // DM key for DigitalEventSeries (if LabelFromEvents)

       // -- Feature conversion --
       ConversionConfig conversion_config;          // NaN dropping, z-score

       // -- Output --
       DimReductionOutputConfig output_config;      // output_key, time_key_str
       std::string time_key_str = "time";
       bool defer_dm_writes = false;
   };
   ```

2. **Create `SupervisedDimReductionPipelineResult`** — extends dim reduction result with label metadata
   - All fields from `DimReductionPipelineResult`
   - `std::vector<std::string> class_names` — discovered class labels
   - `std::size_t num_classes` — number of classes
   - `std::size_t unlabeled_count` — rows without labels (group-based modes)

3. **Create `runSupervisedDimReductionPipeline()`** — orchestration function
   - **Pipeline stages:** ValidatingFeatures → ConvertingFeatures → AssemblingLabels → FittingAndTransforming → WritingOutput → Complete
   - Reuses `FeatureValidator`, `FeatureConverter`, `LabelAssembler` from existing pipelines
   - Calls `MLSupervisedDimReductionOperation::fitTransform()` with features + labels
   - Output column names from `outputColumnNames()` (e.g., "Logit:Contact", "Logit:NoContact")
   - Preserves `RowDescriptor` (TimeFrameIndex rows) from input tensor, filtering for valid indices after NaN/label dropping

4. **Unit tests** — full pipeline integration (labels + features → logit TensorData), row preservation, NaN handling, column naming

**Files to create/edit:**

| File | Action |
|------|--------|
| `src/MLCore/pipelines/SupervisedDimReductionPipeline.hpp` | Create |
| `src/MLCore/pipelines/SupervisedDimReductionPipeline.cpp` | Create |
| `tests/MLCore/test_SupervisedDimReductionPipeline.cpp` | Create |

#### Phase 8d: DimReductionPanel UI — Supervised Mode

**Goal:** Extend the existing `DimReductionPanel` to support supervised algorithms with label source selection.

**Steps:**

1. **Add supervised/unsupervised toggle** — radio buttons or combo at the top of `DimReductionPanel`
   - When "Unsupervised" is selected: show current algorithm list (PCA, t-SNE, Robust PCA)
   - When "Supervised" is selected: show supervised algorithms (Logit Projection, future: LDA)

2. **Add label source configuration** — visible only in supervised mode
   - **Label source type** combo: "Intervals", "Events", "Time Entity Groups", "Data Entity Groups"
   - **Label key** combo: populated dynamically via `updateAllowedValues()` from DataManager
     - "Intervals" → `dm.getKeys<DigitalIntervalSeries>()`
     - "Events" → `dm.getKeys<DigitalEventSeries>()`
     - "Time Entity Groups" / "Data Entity Groups" → group selector from `EntityGroupManager`
   - Uses existing `dynamic_combo` + `include_none_sentinel` pattern from `ParameterSchema`

3. **Wire up `SupervisedDimReductionPipelineWorker`** — background thread execution
   - Same pattern as existing `DimReductionPipelineWorker` but calls `runSupervisedDimReductionPipeline()`
   - Deferred main-thread writes for thread safety

4. **Display class-specific results** — after completion, show:
   - Number of classes and class names discovered
   - Unlabeled row count (for group-based modes)
   - Output tensor shape (N×C)

5. **Auto-focus output** — call `SelectionContext::setDataFocus()` on the output logit tensor
   - Existing `scatter_plot.visualize_2d_tensor` ContextAction handles scatter visualization

**Files to edit:**

| File | Action |
|------|--------|
| `src/WhiskerToolbox/MLCore_Widget/UI/DimReductionPanel/DimReductionPanel.hpp` | Edit: add supervised controls |
| `src/WhiskerToolbox/MLCore_Widget/UI/DimReductionPanel/DimReductionPanel.cpp` | Edit: add supervised logic |
| `src/WhiskerToolbox/MLCore_Widget/UI/DimReductionPanel/DimReductionPanel.ui` | Edit: add supervised UI elements (if .ui exists) |
| `src/WhiskerToolbox/MLCore_Widget/MLCore_Widget.cpp` | Edit: wire supervised pipeline worker |

#### Phase 8e: TransformsV2 Wrapper

**Goal:** Register `TensorLogitProjection` as a container transform for pipeline composability.

**Steps:**

1. **Create `TensorLogitProjection`** — TransformsV2 container transform
   - Input: TensorData (features) + parameter specifying label source key
   - Output: TensorData (logits) with `RowDescriptor` preserved
   - Reflect-cpp parameter struct with `dynamic_combo` for label key selection
   - Static registration via RAII pattern (same as `TensorPCA`, `TensorTSNE`)

2. **`ParameterUIHints` specialization** — annotate `label_key` field as `dynamic_combo`

**Note:** The TransformsV2 wrapper for supervised transforms is more complex than unsupervised wrappers because it needs to specify label source configuration. Consider whether the TransformsV2 parameter struct should embed the full `LabelAssemblyConfig` variant or use a simpler "label_key + label_type" pair. The simpler pair is recommended for TransformsV2 to keep the pipeline JSON format clean.

**Files to create:**

| File | Action |
|------|--------|
| `src/TransformsV2/algorithms/TensorLogitProjection/TensorLogitProjection.hpp` | Create |
| `src/TransformsV2/algorithms/TensorLogitProjection/TensorLogitProjection.cpp` | Create |
| `tests/TransformsV2/test_TensorLogitProjection.cpp` | Create |

#### Phase 8 Design Decisions

| Question | Decision | Rationale |
|----------|----------|-----------|
| Where does the supervised dim reduction base live? | `MLCore/models/MLSupervisedDimReductionOperation.hpp` | Parallel to `MLDimReductionOperation`; same directory |
| Binary vs multi-class handling? | `LogitProjectionOperation` auto-selects: 2 classes → `LogisticRegression`, ≥2 → `SoftmaxRegression` | Single entry point for users; algorithm detail is hidden |
| How are logits extracted from mlpack? | `model.Parameters()` returns weight matrix; logits = $Wx + b$ | Both `LogisticRegression::Parameters()` and `SoftmaxRegression::Parameters()` expose this |
| New `MLTaskType` or reuse? | Add `SupervisedDimensionalityReduction` | Keeps registry queries clean; DimReductionPanel can filter by unsupervised vs supervised |
| How does label key reach the pipeline? | `SupervisedDimReductionPipelineConfig` embeds full `LabelAssemblyConfig` | Reuses proven 4-mode variant from ClassificationPipeline |
| How does label key reach the UI? | `dynamic_combo` + `updateAllowedValues()` on DimReductionPanel | Existing pattern used by DeepLearning_Widget and AutoParamWidget |
| New pipeline or extend existing? | New `SupervisedDimReductionPipeline` | DimReductionPipeline has no label stage; adding one would complicate the unsupervised path |
| Output column naming? | `"Logit:ClassName"` per class | Descriptive; avoids collision with PCA's `"PC1"` naming |

---

## Cross-Widget Communication Patterns (Reference)

### Pattern 1: Transform Output → Scatter Visualization

```
DataTransform_Widget or MLCore Dim Reduction tab
  │ executes TensorPCA → DataManager::setData("features_pca", tensor)
  │ calls SelectionContext::setDataFocus("features_pca", "TensorData", source)
  ▼
ScatterPlotPropertiesWidget (DataFocusAware)
  → onDataFocusChanged → refreshes key combos
  → User selects X=PC1, Y=PC2 (or via "Visualize in Scatter" ContextAction)
  ▼
ScatterPlotOpenGLWidget → rendered scatter with TimeFrameIndex-based points
```

### Pattern 2: Scatter Selection → Entity Group

```
ScatterPlotOpenGLWidget
  → User polygon-selects points → right-click → "Create Group from Selection..."
  → selected_indices → ScatterPointData::time_indices → ensureTimeEntityId()
  → EntityGroupManager::createGroup() + addEntitiesToGroup()
  → observers notified → scatter recolors, GroupManagementWidget refreshes
```

### Pattern 3: Clustering → Scatter Color Update

```
MLCore_Widget → runs K-Means → ClusteringPipeline → EntityGroupManager
  → observers notified → ScatterPlotWidget detects generation counter change
  → scene dirty → applyGroupColorsToScene() → points colored by cluster
```

### Pattern 4: Supervised Dim Reduction → Logit Scatter (Phase 8)

```
DimReductionPanel (supervised mode)
  │ User selects: features="encoder_features", algorithm="Logit Projection"
  │ User selects: label source="Intervals", label_key="contact_labels"
  │ → SupervisedDimReductionPipelineWorker (background thread)
  │    → LabelAssembler: DigitalIntervalSeries → arma::Row<size_t> labels
  │    → LogitProjectionOperation::fitTransform(features, labels, params, logits)
  │    → Creates TensorData (N×C) with columns "Logit:Contact", "Logit:NoContact"
  │    → (deferred write) DataManager::setData("encoder_features_logit", tensor)
  │ → SelectionContext::setDataFocus("encoder_features_logit", "TensorData")
  ▼
ScatterPlotPropertiesWidget (DataFocusAware)
  → onDataFocusChanged → refreshes columns
  → User selects X="Logit:Contact", Y="Logit:NoContact"
  ▼
ScatterPlotOpenGLWidget → class separation visible in logit space
  → Color by entity group shows labeled vs unlabeled
```

---

## Dependency Graph

```
                    ┌────────────────────────────┐
                    │     ScatterPlotWidget       │
                    │  (visualization + selection) │
                    └──────────┬─────────────────┘
                               │ reads TensorData, writes EntityGroups
                               ▼
┌─────────────────┐     ┌──────────────────┐     ┌───────────────────┐
│   TransformsV2  │────▶│   MLCore           │     │ EntityGroupManager│
│ TensorPCA       │     │ PCAOperation       │     │ createGroup()     │
│ TensorTSNE (P3) │     │ TSNEOp (P3)        │     │ notifyObservers() │
│ TensorLogit(P8) │     │ LogitProjection(P8)│────▶│                   │
│ (thin wrappers) │     │ SoftmaxRegression ✅│     └────────┬──────────┘
└────────┬────────┘     │  (P8a done)        │              │
         │              │ Clustering         │              ▼
         ▼              └──────┬─────────────┘     ┌───────────────────┐
┌─────────────────┐            │                   │ SelectionContext   │
│   TensorData    │            ▼                   │ + ContextActions   │
│   (DataObjects) │     ┌──────────────┐           └───────────────────┘
└─────────────────┘     │ Tapkee (P3)  │     ┌───────────────────┐
                        └──────────────┘     │ LabelAssembler    │
                                             │ (Intervals/Events/│
                                             │  Groups) ← P8     │
                                             └───────────────────┘
```

Widget interaction is mediated entirely through DataManager (transforms write output; widgets observe), SelectionContext (`setDataFocus()` broadcasts; `DataFocusAware` auto-updates), and EntityGroupManager (scatter selection creates groups; observers notify all widgets). No widget holds a pointer to any other widget.

---

## Priority Summary

| # | Phase | Priority | Status | Key Deliverable |
|---|-------|----------|--------|-----------------|
| 1 | PCA Foundation | **High** | ✅ Complete | TensorData → TensorData; row preservation |
| 2 | Scatter Selection → Groups | **High** | ✅ Complete | "Create Group from Selection" in scatter |
| 3 | Tapkee (t-SNE, manifold) | **Medium** | **t-SNE complete** | Non-linear dim reduction |
| 4 | MLCore Dim Reduction Tab | **High** | ✅ Complete | Integrated UI; auto-focus output |
| 5 | Clustering ↔ Scatter Loop | **High** | ✅ Complete | ContextAction clustering; selective clustering; label overlay |
| 6 | Elemental Tensor Transforms | Low | Not started | Row-level element transforms |
| 7 | Advanced Features | Low | Not started | Fit/project, scree plot, 3D scatter, benchmarks |
| 8 | Supervised Dim Reduction (Logit Projection) | **High** | **8a complete** | Supervised TensorData → logit TensorData; label integration |

---

## Decision Summary

| Question | Decision | Rationale |
|----------|----------|-----------|
| Where do algorithms live? | **MLCore** | Natural fit/transform API; isolates tapkee dependency; testable independently |
| Where does pipeline integration live? | **TransformsV2** (container transforms) | Full pipeline composability; automatic UI; consistent with existing patterns |
| Initial backend? | **mlpack PCA** (already a dependency) | Zero new dependencies for Phase 1 |
| Manifold methods backend? | **Tapkee** (Phase 3) | Best C++ manifold learning library; header-only |
| Cross-widget communication? | **SelectionContext + EntityGroupManager + DataManager observers** | All infrastructure exists; no widget-to-widget pointers |
| How do scatter selections become groups? | **Context menu → EntityGroupManager** | Scatter already has selection + GroupManager; add write path |
| Elemental tensor transforms? | **Phase 6** (deferred) | Requires ContainerTraits work for TensorData |
| Separate TensorTransforms library? | **No** | Hybrid approach avoids new infrastructure |
| Supervised dim reduction: new pipeline or extend existing? | **New `SupervisedDimReductionPipeline`** | DimReductionPipeline is unsupervised-only; label assembly doesn't belong there |
| Logit projection: binary vs softmax handling? | **Auto-select in `LogitProjectionOperation`** | 2 classes → `LogisticRegression`, ≥2 → `SoftmaxRegression`; single user-facing entry |
| Label key in UI? | **`dynamic_combo` + `updateAllowedValues()`** | Proven pattern from DeepLearning_Widget; populates from DataManager at runtime |

## See Also

- [Inter-Widget Communication Roadmap](inter_widget_communication_roadmap.md) — ContextActions, OperationContext, cross-roadmap sequence
- [Command Architecture Roadmap](DataManager/Commands/roadmap.qmd) — command registration, CommandRecorder integration
- [Command Architecture Design Document](DataManager/Commands/index.qmd) — full types and patterns

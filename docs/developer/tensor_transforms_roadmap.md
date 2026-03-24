# Tensor Transform Library — Design Roadmap

**Last updated:** 2025-07-22

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
| t-SNE / manifold methods | tapkee integration | Phase 3 (next) |
| Scatter plot of reduced features | ScatterPlotWidget (TensorData columns) | ✅ Exists |
| Color points by group | ScatterPlotWidget (`colorByGroup` + GroupManager) | ✅ Exists |
| Select points → create group | ScatterPlotWidget → EntityGroupManager | ✅ Phase 2 complete |
| Cluster automatically | MLCore ClusteringPipeline + ContextAction | ✅ Phase 5 complete |
| Propagate groups to other widgets | SelectionContext + DataFocusAware | ✅ Exists |
| Dim reduction UI | MLCore_Widget Dim Reduction tab | ✅ Phase 4 complete |
| Scatter ↔ clustering loop | ContextActions + selective clustering + label overlay | ✅ Phase 5 complete |

---

## Architecture: Hybrid (Option D)

**Decision:** Algorithms live in MLCore; pipeline integration lives in TransformsV2. This was chosen over three alternatives (pure TransformsV2, standalone TensorTransforms library, pure MLCore). See the Decision Summary at the end of this document for rationale.

```
MLCore (algorithms)                TransformsV2 (pipeline wrappers)
├── MLDimReductionOperation        ├── TensorPCA (container transform)
├── PCAOperation (mlpack)          ├── TensorTSNE (Phase 3)
├── TSNEOperation (Phase 3)        └── TensorIsomap (Phase 3)
├── DimReductionPipeline           
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

### Phase 3: Tapkee Integration — NEXT

**Goal:** Non-linear dimensionality reduction (t-SNE, manifold methods).

**Note:** `DimReductionPipeline` already exists from Phase 4 — only tapkee-specific operations and TransformsV2 wrappers need to be added.

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
│ TensorPCA       │     │ PCAOperation │     │ createGroup()     │
│ TensorTSNE (P3) │     │ TSNEOp (P3)  │     │ notifyObservers() │
│ (thin wrappers) │     │ Clustering   │────▶│                   │
└────────┬────────┘     └──────┬───────┘     └────────┬──────────┘
         │                     │                      │
         ▼                     ▼                      ▼
┌─────────────────┐     ┌──────────────┐     ┌───────────────────┐
│   TensorData    │     │ Tapkee (P3)  │     │ SelectionContext   │
│   (DataObjects) │     │ (header-only)│     │ + ContextActions   │
└─────────────────┘     └──────────────┘     └───────────────────┘
```

Widget interaction is mediated entirely through DataManager (transforms write output; widgets observe), SelectionContext (`setDataFocus()` broadcasts; `DataFocusAware` auto-updates), and EntityGroupManager (scatter selection creates groups; observers notify all widgets). No widget holds a pointer to any other widget.

---

## Priority Summary

| # | Phase | Priority | Status | Key Deliverable |
|---|-------|----------|--------|-----------------|
| 1 | PCA Foundation | **High** | ✅ Complete | TensorData → TensorData; row preservation |
| 2 | Scatter Selection → Groups | **High** | ✅ Complete | "Create Group from Selection" in scatter |
| 3 | Tapkee (t-SNE, manifold) | **Medium** | **Next** | Non-linear dim reduction |
| 4 | MLCore Dim Reduction Tab | **High** | ✅ Complete | Integrated UI; auto-focus output |
| 5 | Clustering ↔ Scatter Loop | **High** | ✅ Complete | ContextAction clustering; selective clustering; label overlay |
| 6 | Elemental Tensor Transforms | Low | Not started | Row-level element transforms |
| 7 | Advanced Features | Low | Not started | Fit/project, scree plot, 3D scatter, benchmarks |

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

## See Also

- [Inter-Widget Communication Roadmap](inter_widget_communication_roadmap.md) — ContextActions, OperationContext, cross-roadmap sequence
- [Command Architecture Roadmap](DataManager/Commands/roadmap.qmd) — command registration, CommandRecorder integration
- [Command Architecture Design Document](DataManager/Commands/index.qmd) — full types and patterns

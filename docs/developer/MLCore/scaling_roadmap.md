# MLCore Feature Scaling Roadmap

**Related documents:**
- [Precondition Documentation Protocol](../analysis/precondition_protocol.qmd) — Step 8 applies this protocol to ML operation interfaces
- `src/MLCore/features/FeatureConverter.hpp` — the single normalization entry point after this refactor

## Motivation

Feature scaling (z-score normalization) is currently handled inconsistently across MLCore algorithms. Some algorithms perform internal scaling silently, others don't, and the pre-algorithm z-score checkbox interacts with per-algorithm scaling in confusing ways. This leads to:

- **Double-scaling risk**: User checks "Z-score normalize" AND PCA/Supervised PCA default `scale=true` → data is standardized twice.
- **Inconsistent defaults**: Switching from PCA (scales by default) to Robust PCA (no scaling) silently drops scaling.
- **Missing scaling option**: Classification algorithms (Logistic Regression, Softmax, HMM) have no z-score option at all, despite being scale-sensitive.
- **Mixed normalization bases**: When z-score uses all-data statistics but Supervised PCA uses labeled-subset statistics.

## Current State Audit

### Two Independent Scaling Mechanisms

1. **Pre-algorithm z-score** (`ConversionConfig::zscore_normalize`) — applied in `FeatureConverter` before data reaches any algorithm.
2. **Per-algorithm internal scaling** — `PCAParameters::scale`, `SupervisedPCAParameters::scale`, `LogitProjectionParameters::scale_features`.

### Per-Algorithm Status

| Algorithm | Z-score checkbox? | Internal scaling? | Internal default | Double-scale risk |
|---|---|---|---|---|
| PCA | Yes (off by default) | ~~Yes (`scale`)~~ **No (centering only)** ✓ | N/A | **No** ✓ |
| Robust PCA | Yes (off by default) | No (centering only) | N/A | No |
| t-SNE | Yes (off by default) | No | N/A | No |
| Supervised PCA | Yes (off by default) | ~~Yes (`scale`)~~ **No (centering only)** ✓ | N/A | **No** ✓ |
| Supervised Robust PCA | Yes (off by default) | No (centering only) | N/A | No |
| Logit Projection | Yes (off by default) | ~~Yes (`scale_features`)~~ **No** ✓ | N/A | **No** ✓ |
| K-Means | Yes (off by default) | No | N/A | No |
| DBSCAN | Yes (off by default) | No | N/A | No |
| GMM | Yes (off by default) | No | N/A | No |
| Random Forest | **No** (hardcoded off) | No | N/A | No |
| Naive Bayes | **No** (hardcoded off) | No | N/A | No |
| Logistic Regression | **No** (hardcoded off) | No | N/A | No |
| Softmax Regression | **No** (hardcoded off) | No | N/A | No |
| HMM | **No** (hardcoded off) | No | N/A | No |

### Key Files

- `src/MLCore/features/FeatureConverter.hpp` — `ConversionConfig::zscore_normalize`, `zscoreNormalize()`, `applyZscoreNormalization()`
- `src/MLCore/models/MLModelParameters.hpp` — ~~`PCAParameters::scale`~~, ~~`SupervisedPCAParameters::scale`~~, `LogitProjectionParameters::scale_features`
- `src/MLCore/models/unsupervised/PCAOperation.cpp` — ~~Internal centering + scaling~~ **Centering only** ✓
- `src/MLCore/models/supervised/SupervisedPCAOperation.cpp` — ~~Internal centering + scaling from labeled subset~~ **Centering only (labeled-subset mean)** ✓
- `src/MLCore/models/supervised/LogitProjectionOperation.cpp` — ~~Internal `computeScaling()`/`applyScaling()` + weight rescaling~~ **No internal scaling** ✓
- `src/WhiskerToolbox/MLCore_Widget/UI/DimReductionPanel/DimReductionPanel.cpp` — Z-score checkbox only (scale checkbox removed ✓)
- `src/WhiskerToolbox/MLCore_Widget/MLCoreWidget.cpp` — Pipeline config assembly (lines 760-761 hardcode classification zscore off)

## Design Goal

**Single responsibility**: Feature scaling is a **precondition** handled once at the pipeline input stage, not inside individual algorithms.

- The z-score normalize checkbox becomes the **sole** scaling control.
- Algorithms receive data that is either scaled or not, and do not re-scale.
- All algorithms center (subtract mean) internally as needed — centering is mathematically distinct from scaling and is inherent to PCA/Robust PCA.

## Implementation Steps

### ~~Step 1: Remove internal scaling from PCA~~ ✓ DONE

**Files**: `PCAOperation.cpp`, `PCAOperation.hpp`, `MLModelParameters.hpp`

**Completed.** Changes made:

- Removed `PCAParameters::scale` field from `MLModelParameters.hpp`.
- Removed `_impl->stddev` and `_impl->scaled` fields from `PCAOperation::Impl`.
- Removed stddev computation and conditional scaling from `fitTransform()` and `transform()`.
- Updated `save()`/`load()` — no longer serializes `scaled` or `stddev`. Old saved models are incompatible.
- Updated `PCAOperation.hpp` doc block to explicitly state that scaling is the caller's responsibility.
- Removed `params->scale` assignment in `DimReductionPanel::currentParameters()` for PCA.
- Removed `TensorPCAParams::scale` field and its usage in `TensorPCA.cpp`.
- Removed/rewrote obsolete scale-related test cases in `PCAOperation.test.cpp` and `PCA_HMM_Equivalence.test.cpp`.
- Build clean, all tests pass.

### ~~Step 2: Remove internal scaling from Supervised PCA~~ ✓ DONE

**Files**: `SupervisedPCAOperation.cpp`, `SupervisedPCAOperation.hpp`, `MLModelParameters.hpp`

**Completed.** Changes made:

- Removed `SupervisedPCAParameters::scale` field from `MLModelParameters.hpp`.
- Removed `_impl->stddev` and `_impl->scaled` fields from `SupervisedPCAOperation::Impl`.
- Removed stddev computation and conditional scaling from `fitTransform()` and `transform()`.
- Made `centered` and `all_centered` local variables `const` (clang-tidy).
- Updated `save()`/`load()` — no longer serializes `scaled` or `stddev`. Old saved models are incompatible.
- Updated `SupervisedPCAOperation.hpp` doc block to state that scaling is the caller's responsibility.
- Removed `params->scale = scaleFeatures()` in `DimReductionPanel::currentParameters()` for Supervised PCA.
- Removed/updated `params.scale` lines in `SupervisedPCAOperation.test.cpp`.
- Build clean, all tests pass.

### ~~Step 3: Remove internal scaling from Logit Projection~~ ✓ DONE

**Files**: `LogitProjectionOperation.cpp`, `LogitProjectionOperation.hpp`, `MLModelParameters.hpp`

**Completed.** Changes made:

- Removed `LogitProjectionParameters::scale_features` field from `MLModelParameters.hpp`.
- Removed the anonymous-namespace `ScalingParams`, `computeScaling()`, and `applyScaling()` helpers from `LogitProjectionOperation.cpp`.
- Removed `bool const scale = lp_params->scale_features` and the conditional scaling block in `fitTransform()`.
- Removed the weight matrix / bias rescaling block (`_impl->weight_matrix.each_row() /= scaling.std.t()` etc.) — `fitTransform()` now trains directly on the input features.
- `transform()` unchanged — already computed `W * X + b` with no scaling logic.
- `save()`/`load()` unchanged — no scaling fields were ever serialized.
- Removed `scale_features=true` variant from doc comment and test file; removed `CHECK(lp_params->scale_features == false)` from metadata test.
- Updated `LogitProjectionOperation.hpp` class doc to state that scaling is the caller's responsibility.
- Build clean, all tests pass.

### ~~Step 4: Remove per-algorithm "Scale features" checkbox from UI~~ ✓ DONE

**Files**: `DimReductionPanel.cpp`, `DimReductionPanel.ui`, `DimReductionPanel.hpp`, `MLCoreWidgetState.hpp`, `MLCoreWidgetState.cpp`, `MLCoreWidgetStateData.hpp`

**Completed.** Changes made:

- Removed `scaleCheckBox` widget from `DimReductionPanel.ui` (was row 1 of `pcaParamsWidget` grid).
- Removed `DimReductionPanel::scaleFeatures()` declaration and implementation.
- Removed `connect(ui->scaleCheckBox ...)` signal connection.
- Removed `ui->scaleCheckBox->setChecked(...)` in `_restoreFromState()`.
- Removed `_state->setDimReductionScale(scaleFeatures())` from `_syncToState()`.
- Removed `bool dim_reduction_scale` field from `MLCoreWidgetStateData`.
- Removed `setDimReductionScale()`/`dimReductionScale()` getter/setter and `dimReductionScaleChanged` signal from `MLCoreWidgetState`.
- Removed the `emit dimReductionScaleChanged(...)` call in `fromJson()`.
- `currentParameters()` was already clean — no `params->scale` assignments remained after Steps 1–2.
- Build clean, all tests pass.

### ~~Step 5: Default z-score normalize to ON for Dim Reduction~~ ✓ DONE

**Files**: `MLCoreWidgetStateData.hpp`, `DimReductionPanel.ui`

**Completed.** Changes made:

- Changed `dim_reduction_zscore_normalize` default from `false` to `true` in `MLCoreWidgetStateData.hpp`.
- Changed `zscoreCheckBox` `checked` property from `false` to `true` in `DimReductionPanel.ui` so the checkbox initializes checked on first launch (before any saved state is restored).
- Existing persisted workspace state continues to override the default, so users with saved sessions keep their setting.
- Build clean.

### Step 6: Expose z-score normalize for Classification tab ✓ DONE

**Files**: `MLCoreWidget.cpp`, `FeatureSelectionPanel.hpp`/`.cpp`/`.ui`, `MLCoreWidgetState.hpp`/`.cpp`, `MLCoreWidgetStateData.hpp`

No separate ClassificationPanel exists — the classification tab is assembled from sub-panels directly in `MLCoreWidget`. The z-score checkbox was added to `FeatureSelectionPanel` (the data source panel), matching the pattern in `ClusteringPanel` and `DimReductionPanel`.

**Changes applied:**

1. `FeatureSelectionPanel.ui`: Added `zscoreCheckBox` (checked=true, with tooltip) after `hintLabel` in `featureGroupBox`.
2. `MLCoreWidgetStateData.hpp`: Added `bool classification_zscore_normalize = true;` after `feature_tensor_key`.
3. `MLCoreWidgetState.hpp/.cpp`: Added `setClassificationZscoreNormalize()`/`classificationZscoreNormalize()` getter/setter and `classificationZscoreNormalizeChanged` signal; `fromJson()` emits the signal on restore.
4. `FeatureSelectionPanel.hpp/.cpp`: Added `zscoreNormalize()` accessor; constructor restores checkbox from state; `_setupConnections()` wires checkbox → state (toggled) and state → checkbox (signal).
5. `MLCoreWidget.cpp` (line ~761): Changed `zscore_normalize = false` → `zscore_normalize = _feature_panel->zscoreNormalize()`.

Build clean, clang-tidy clean (2 pre-existing warnings unrelated to these changes).

### Step 7: Expose z-score normalize for Sequence (HMM) tab — N/A (deferred)

No separate HMM/Sequence tab exists. HMM models are trained through the **Classification** tab via `isSequenceModel()` in `PredictionPanel`. The Classification pipeline already reads z-score from `_feature_panel->zscoreNormalize()` (Step 6). No additional changes are needed here.

If a dedicated Sequence tab is added in future, it should follow the same pattern: a SequencePanel with a `zscoreCheckBox`, a `sequence_zscore_normalize` state field, and a `_buildSequencePipelineConfig()` reading from the panel.

### Step 8: Precondition documentation (following Precondition Protocol) ✓ DONE

Apply the [Precondition Documentation Protocol](../analysis/precondition_protocol.qmd) to all
MLCore operation interfaces. This step is documentation-only — no asserts or control flow changes.

**Progress:**

| Header | Functions annotated | Status |
|---|---|---|
| `PCAOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `SupervisedPCAOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `LogitProjectionOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `MLDimReductionOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `MLSupervisedDimReductionOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `MLModelOperation.hpp` | `train()`, `predict()`, `fit()`, `assignClusters()` | ✓ Done |
| `FeatureConverter.hpp` | `convertTensorToArma()` | ✓ Done (`@post` added) |
| `DimReductionPipeline.hpp` | `runDimReductionPipeline()` | ✓ Done (`@pre` propagated) |
| `SupervisedDimReductionPipeline.hpp` | `runSupervisedDimReductionPipeline()` | ✓ Done (`@pre` propagated) |
| `ClassificationPipeline.hpp` | `runClassificationPipeline()` | ✓ Done (`@pre` propagated) |
| `ClusteringPipeline.hpp` | `runClusteringPipeline()` | ✓ Done (`@pre` propagated) |

**Caller propagation findings from `PCAOperation` analysis:**

| Caller | File | NaN Precondition established? | Action |
|---|---|---|---|
| `DimReductionPipeline` | `DimReductionPipeline.cpp:274` | Yes — `convertTensorToArma` drops NaN/Inf | None |
| `SupervisedDimReductionPipeline` (fitTransform) | `SupervisedDimReductionPipeline.cpp:438` | Yes — same conversion path | None |
| `SupervisedDimReductionPipeline` (transform) | `SupervisedDimReductionPipeline.cpp:447` | Yes — same converted matrix | None |
| `tensorPCA` (TransformsV2) | `TensorPCA.cpp:65` | **No** — raw tensor, no NaN filtering | Bug: add NaN check |
| `tensorTSNE` (TransformsV2) | `TensorTSNE.cpp:66` | **No** — same pattern | Bug: add NaN check |
| `tensorRobustPCA` (TransformsV2) | `TensorRobustPCA.cpp:67` | **No** — same pattern | Bug: add NaN check |

**Caller propagation findings from `LogitProjectionOperation` analysis:**

| Caller | File | Precondition Status | Action |
|---|---|---|---|
| `SupervisedDimReductionPipeline` (fitTransform) | `SupervisedDimReductionPipeline.cpp:438` | features not empty: **pass-through** (not explicitly checked before call); labels match: **guaranteed** by parallel subset ops | Add empty guard before fit call |
| `SupervisedDimReductionPipeline` (transform) | `SupervisedDimReductionPipeline.cpp:447` | `isTrained()`: **guaranteed** (successful fitTransform precedes); dimension match: **guaranteed** (same FeatureConverter output) | None critical |
| `SupervisedDimReductionPipeline` (both) | Both sites | NaN/Inf: **pass-through** — FeatureConverter drops NaN only when `remove_nan=true` | NaN guard is future hardening |

**Remaining target interfaces**: None — all headers annotated.

**Applied `@pre` tags** (with enforcement classification per protocol):

For `MLDimReductionOperation::fitTransform()` and `MLSupervisedDimReductionOperation::fitTransform()`:
```cpp
/// @pre features must not be empty (enforcement: runtime_check)
/// @pre Features are expected to be z-score normalized (mean≈0, std≈1 per feature)
///      when scale-sensitive comparison across features is desired.
///      Normalization is the caller's responsibility (via FeatureConverter).
///      Centering is handled internally by each algorithm. (enforcement: none) [IMPORTANT]
```

For `MLDimReductionOperation::transform()` and `MLSupervisedDimReductionOperation::transform()`:
```cpp
/// @pre isTrained() must be true (enforcement: runtime_check)
/// @pre features.n_rows must equal numFeatures() (enforcement: runtime_check)
/// @pre Features must be z-score normalized using the SAME parameters (mean/std)
///      that were used for the training data passed to fitTransform().
///      Use FeatureConverter::applyZscoreNormalization() with stored parameters.
///      (enforcement: none) [IMPORTANT]
```

For `MLModelOperation::train()` and `MLModelOperation::predict()` (classification):
```cpp
/// @pre Features are expected to be z-score normalized for scale-sensitive
///      classifiers (Logistic Regression, Softmax Regression).
///      Tree-based methods (Random Forest) are scale-invariant.
///      (enforcement: none) [IMPORTANT]
```

**Caller propagation** (Step 6 of protocol) — all actions completed:

| Caller | File | Precondition Status | Action |
|---|---|---|---|
| `runDimReductionPipeline()` | `DimReductionPipeline.hpp` | **Establishes** — calls `convertTensorToArma()` with `ConversionConfig` | ✓ `@post` on `convertTensorToArma`, `@pre` on pipeline |
| `runSupervisedDimReductionPipeline()` | `SupervisedDimReductionPipeline.hpp` | **Establishes** — same pattern | ✓ `@pre` on pipeline |
| `runClassificationPipeline()` | `ClassificationPipeline.hpp` | **Establishes** — reads from config | ✓ `@pre` on pipeline |
| `runClusteringPipeline()` | `ClusteringPipeline.hpp` | **Establishes** — reads from config | ✓ `@pre` on pipeline |

`@post` added to `convertTensorToArma()` in `FeatureConverter.hpp`:
```cpp
/// @post If config.drop_nan is true, result.matrix contains no NaN or Inf values
/// @post If config.zscore_normalize is true, each row (feature) of result.matrix
///       has mean≈0 and std≈1, and result.zscore_means / result.zscore_stds are
///       populated for re-application to new data via applyZscoreNormalization()
/// @post result.valid_row_indices maps each matrix column back to the original
///       tensor row index
```

### Step 9: Enforcement in tests

**Files**: `tests/MLCore/` test files

The `(enforcement: none) [IMPORTANT]` precondition for z-score normalization cannot be
enforced via `assert()` at runtime (data could legitimately be unscaled for tree-based
methods). Instead, enforce via **test coverage**:

1. **Pipeline-level integration tests**: Verify that when `zscore_normalize=true`, the matrix
   passed to `fitTransform()` has per-feature mean ≈ 0 and std ≈ 1. This can be checked by
   intercepting the matrix in a test harness or by adding a `FeatureConverter` unit test:

   ```cpp
   TEST_CASE("convertTensorToArma z-score produces unit-variance features") {
       // Build a TensorData with known non-unit-variance columns
       // Convert with zscore_normalize=true
       // Check: for each row of result.matrix, mean ≈ 0 and stddev ≈ 1
   }
   ```

2. **Double-scaling regression test**: Verify that after Steps 1-3, running PCA on z-score
   normalized data produces the same result as the old `scale=true` path on raw data:

   ```cpp
   TEST_CASE("PCA on z-scored input matches old scale=true behavior") {
       // 1. Z-score normalize raw features manually
       // 2. Run PCA (no internal scaling — scale field removed)
       // 3. Compare eigenvalues/eigenvectors to reference
   }
   ```

3. **Algorithm parameter removal tests**: Update existing PCA, Supervised PCA, and Logit
   Projection tests that pass `scale=true` or `scale_features=true` — remove those parameters
   since the fields no longer exist.

4. **Invariance tests for scale-insensitive methods**: Verify that Random Forest and Naive
   Bayes produce identical results with and without z-score normalization (confirming they
   are truly scale-invariant).

5. **Robustness**: Verify that Robust PCA, t-SNE, and all classification algorithms work
   correctly with pre-scaled input (no NaN, no crashes, reasonable output).

### Step 10: Update developer documentation ✓ DONE

**Files**: `docs/developer/MLCore/dim_reduction.qmd`, `features.qmd`, `models.qmd`, `logit_projection.qmd`, `supervised_pca.qmd`, `widget.qmd`

**Completed.** Changes made:

1. **`dim_reduction.qmd`**: Removed `scale` from PCA parameters table, PCA algorithm steps, and both `TensorPCAParams` tables. Added note that scaling is the caller's responsibility with link to roadmap.
2. **`features.qmd`**: Noted z-score normalization is the recommended default and sole scaling mechanism. Added context about UI checkbox defaults.
3. **`models.qmd`**: Added "Feature Scaling Precondition" section explaining that scaling is a pipeline-level concern with link to roadmap.
4. **`logit_projection.qmd`**: Removed `scale_features` from parameters table, removed "Feature Scaling Absorption" section (no longer applicable), updated code example, updated test description.
5. **`supervised_pca.qmd`**: Removed `scale` from parameters table, updated algorithm steps to remove optional scaling, removed `stddev` from Impl struct description, updated `transform()` description.
6. **`widget.qmd`**: Removed `dim_reduction_scale` from state fields table (field was deleted in Step 4), removed "Scale features" checkbox from UI diagram, updated `DimReductionPanel` description.

## Migration Notes

- **Serialized models**: Existing saved PCA models (via `PCAOperation::save()`) store `scaled` and `stddev` fields. The `load()` method should handle legacy formats gracefully — if `scaled=true` is found in a saved model, `transform()` should still apply the stored stddev for backward compatibility. Consider a version field or try/catch during deserialization.
- **User-visible behavior change**: PCA results will differ slightly if a user was running PCA with `scale=true` (default) but z-score checkbox unchecked. After this change, they would need to check the z-score box to get equivalent behavior. This should be noted in release notes.

## Centering and Z-Score Interaction

Z-score normalization (`(x - mean) / std`) performs both centering (subtract mean) and scaling
(divide by std). This means if z-score is applied pre-algorithm, the data arriving at PCA is
already centered. PCA's internal centering then computes `mean ≈ 0` and subtracts it — a near
no-op on already-centered data.

**Why keep internal centering anyway:**

1. **Z-score is optional.** The user can uncheck the z-score box. When they do, PCA's internal
   centering is essential for correctness (PCA on un-centered data produces wrong eigenvectors).
2. **Idempotent.** Centering already-centered data is a no-op (subtracts ~0). It costs nothing
   and causes no harm.
3. **Supervised methods have a different centering basis.** Supervised PCA centers on the
   labeled subset, not all data. Even if z-score was computed on all data (making the global
   mean ≈ 0), the labeled subset's mean may differ. This subset-specific centering is
   semantically meaningful for supervised algorithms.
4. **Model serialization.** The stored `mean` vector is needed for `transform()` on new data.
   Removing internal centering would break the projection pipeline.

**Bottom line:** Internal centering stays in all PCA-family algorithms. It is harmless when
z-score is on, essential when z-score is off, and required for projection of new data.

## Out of Scope

- **Min-max normalization**: Not currently offered; could be added as a future `ConversionConfig` option.
- **Per-feature selection of scaling**: All features are scaled uniformly. Not needed for current use cases.

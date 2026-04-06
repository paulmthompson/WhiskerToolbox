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
- `src/WhiskerToolbox/MLCore_Widget/UI/DimReductionPanel/DimReductionPanel.cpp` — Z-score checkbox + per-algorithm scale checkbox
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

### Step 4: Remove per-algorithm "Scale features" checkbox from UI

**Files**: `DimReductionPanel.cpp`, `DimReductionPanel.ui`, `DimReductionPanel.hpp`, `MLCoreWidgetState.hpp`, `MLCoreWidgetState.cpp`

1. Remove the `scaleCheckBox` from the `.ui` file (inside `pcaParamsWidget`).
2. Remove `DimReductionPanel::scaleFeatures()` method.
3. Remove `MLCoreWidgetState::dim_reduction_scale` and its getter/setter/signal.
4. In `DimReductionPanel::currentParameters()`:
   - For PCA: stop setting `params->scale` (field no longer exists).
   - For Supervised PCA: same.
5. Remove state save/restore code for `dim_reduction_scale`.

### Step 5: Default z-score normalize to ON for Dim Reduction

**Files**: `MLCoreWidgetState.hpp` (or its data struct), `DimReductionPanel.cpp`

1. Change the default value of `dim_reduction_zscore_normalize` from `false` to `true`.
2. Ensure the checkbox in `DimReductionPanel` reflects this default on first launch.
3. Existing persisted state will still override the default, so users with saved sessions keep their setting.

### Step 6: Expose z-score normalize for Classification tab

**Files**: `MLCoreWidget.cpp`, `ClassificationPanel.hpp`/`.cpp`/`.ui` (or `LabelPanel`), `MLCoreWidgetState.hpp`/`.cpp`

Currently classification hardcodes `config.conversion_config.zscore_normalize = false` (line 761 in `MLCoreWidget.cpp`).

1. Add a z-score checkbox to the classification panel UI (matching the pattern in Clustering/DimReduction panels).
2. Add `classification_zscore_normalize` state field to `MLCoreWidgetState`.
3. Wire the checkbox to state and read it during pipeline config assembly.
4. Default to `true`.

### Step 7: Expose z-score normalize for Sequence (HMM) tab

**Files**: `MLCoreWidget.cpp`, `SequencePanel.hpp`/`.cpp`/`.ui`, `MLCoreWidgetState.hpp`/`.cpp`

Same pattern as Step 6 for the sequence/HMM workflow. HMM with Gaussian emissions is sensitive to feature scale.

### Step 8: Precondition documentation (following Precondition Protocol) — PARTIAL

Apply the [Precondition Documentation Protocol](../analysis/precondition_protocol.qmd) to all
MLCore operation interfaces. This step is documentation-only — no asserts or control flow changes.

**Progress:**

| Header | Functions annotated | Status |
|---|---|---|
| `PCAOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `SupervisedPCAOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `LogitProjectionOperation.hpp` | `fitTransform()`, `transform()` | ✓ Done |
| `MLDimReductionOperation.hpp` | `fitTransform()`, `transform()` | `fitTransform` has partial base-class `@pre`; `transform` has minimal `@pre` — needs expansion |
| `MLSupervisedDimReductionOperation.hpp` | `fitTransform()`, `transform()` | Not started |
| `MLModelOperation.hpp` | `train()`, `predict()`, `fit()`, `assignClusters()` | Not started |

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

**Remaining target interfaces** (headers only):

| Header | Functions to annotate |
|---|---|
| `MLDimReductionOperation.hpp` | `fitTransform()`, `transform()` (expand existing) |
| `MLSupervisedDimReductionOperation.hpp` | `fitTransform()`, `transform()` |
| `MLModelOperation.hpp` | `train()`, `predict()`, `fit()`, `assignClusters()` |

**New `@pre` tags to add** (with enforcement classification per protocol):

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

**Caller propagation** (Step 6 of protocol):

Trace callers to verify the precondition is established:

| Caller | File | Precondition Status | Action |
|---|---|---|---|
| `runDimReductionPipeline()` | `DimReductionPipeline.cpp` | **Establishes** — calls `convertTensorToArma()` with `ConversionConfig` | Document `@post` on `convertTensorToArma` |
| `runSupervisedDimReductionPipeline()` | `SupervisedDimReductionPipeline.cpp` | **Establishes** — same pattern | Document `@post` |
| `runClassificationPipeline()` | `ClassificationPipeline.cpp` | **Pass-through** — reads from config | Needs `@pre` propagated |
| `runClusteringPipeline()` | `ClusteringPipeline.cpp` | **Establishes** — reads from config | Document `@post` |

Also add a `@post` to `convertTensorToArma()`:
```cpp
/// @post If config.zscore_normalize is true, result.matrix has per-feature
///       mean≈0 and std≈1, and result.zscore_means/zscore_stds are populated
///       for re-application to new data.
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

### Step 10: Update developer documentation

**Files**: `docs/developer/MLCore/dim_reduction.qmd`, `features.qmd`, `models.qmd`

1. Document that scaling is a pipeline-level concern, not an algorithm-level concern.
2. Update the FeatureConverter documentation to note that z-score normalization is the recommended default.
3. Remove references to per-algorithm `scale` parameters from algorithm docs.
4. Reference the precondition protocol and link to this roadmap for the rationale.

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

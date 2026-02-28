# Rate Estimation Library — Redesign Proposal

## Current Problems

### 1. `RateProfile` conflates binning metadata with general output

`RateProfile` stores `bin_start` and `bin_width`, which are binning-specific concepts. A Gaussian kernel estimator evaluates the rate at arbitrary time points — it doesn't have "bins." An adaptive method might produce irregularly spaced evaluation points. The output struct shouldn't encode assumptions about how the rate was computed.

### 2. No first-class time axis

The consumer always needs a paired `(time[], rate[])` for plotting, but `RateProfile` forces callers to reconstruct the time vector from `bin_start + i * bin_width`. This is fragile and doesn't generalize to non-uniform evaluation grids.

### 3. Normalization is tangled with rate metadata

`normalizeRateProfiles()` takes `RateProfile` (which carries `num_trials`) and produces `NormalizedRow` (a separate struct with redundant `bin_start`/`bin_width`). This means:
- Two nearly-identical structs exist (`RateProfile` → `NormalizedRow` → `HeatmapRowData`)
- The normalization function can't be applied to already-normalized data or arbitrary value vectors
- Z-score internally calls `toCountPerTrial()` first — the normalization pipeline has hidden coupling to the raw count representation

### 4. No path to uncertainty quantification

The current pipeline throws away per-trial information immediately: `binningEstimate()` sums across all trials into a single histogram. There's no way to compute SEM, confidence intervals, or bootstrap statistics without re-architecting how estimation works.

### 5. `NormalizedRow` duplicates `HeatmapRowData`

The roadmap acknowledges this: "This struct intentionally mirrors `CorePlotting::HeatmapRowData`." Having two identical structs is a dependency-avoidance hack but also a maintenance burden.

### 6. Scaling enum is heatmap-specific

`HeatmapScaling` is used by the heatmap widget's state, but the operations it represents (Hz conversion, z-score, min-max) are generic. A PSTH widget would need a near-identical `PSTHNormalization` enum as the roadmap already sketches.

---

## Design Goals

1. **Method-agnostic output**: The output of any estimation method should be a simple `(times, values)` pair, optionally with uncertainty bands.
2. **Per-trial access**: The estimation layer should be able to produce per-trial rate curves (for variability/CI computation) without duplicating the estimation logic.
3. **Composable normalization**: Normalization transforms should operate on any `(times, values)` data, not just `RateProfile`.
4. **Extensible estimation**: Adding Gaussian kernel smoothing or adaptive methods should require adding one function, not modifying every downstream struct.
5. **Uncertainty as a first-class concept**: Error bars / confidence bands should be representable in the output without bolting them on later.

---

## Proposed Architecture

### Layer Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    Widget Layer                          │
│  HeatmapOpenGLWidget / PSTHPlotOpenGLWidget             │
│  Consumes RateEstimate, feeds mappers/renderers         │
└────────────┬────────────────────────┬───────────────────┘
             │                        │
     ┌───────▼────────┐    ┌─────────▼──────────┐
     │  Normalization  │    │  Uncertainty (opt)  │
     │  normalize()    │    │  computeCI()        │
     │  zScore()       │    │  bootstrap()        │
     │  minMax01()     │    │                     │
     └───────┬────────┘    └─────────┬──────────┘
             │                        │
     ┌───────▼────────────────────────▼──────────┐
     │            RateEstimate (output)           │
     │  std::vector<double> times                 │
     │  std::vector<double> values                │
     │  size_t num_trials                         │
     │  std::optional<PerTrialData> per_trial     │
     └───────────────────┬───────────────────────┘
                         │
     ┌───────────────────▼───────────────────────┐
     │          Estimator Functions                │
     │  estimateBinned(gathered, tf, window, p)   │
     │  estimateKernel(gathered, tf, window, p)   │
     │  estimateAdaptive(gathered, tf, window, p) │
     └───────────────────┬───────────────────────┘
                         │
     ┌───────────────────▼───────────────────────┐
     │        UnitGatherContext (unchanged)        │
     │  key, gathered, time_frame                  │
     └───────────────────────────────────────────┘
```

### Core Output: `RateEstimate`

```cpp
/// The universal output of any rate estimation method.
/// Always a paired (times, values) suitable for direct plotting.
struct RateEstimate {
    std::vector<double> times;      ///< Evaluation time points (relative to alignment)
    std::vector<double> values;     ///< Estimated values at each time point
                                    ///  (raw count, rate, or whatever the estimator produces)
    size_t num_trials = 0;          ///< Trials that contributed to the estimate
};
```

**Why this is better:**
- `times[i], values[i]` is immediately plottable — no reconstruction needed.
- For binning, `times` are **bin centers** (e.g., a 10 ms bin covering `[-100, -90)` reports `times[i] = -95`). Consumers that need rectangle geometry (like the heatmap mapper) can reconstruct left edges from the metadata's `sample_spacing`.
- For kernel smoothing, `times` would be a regular evaluation grid (e.g., every 1 ms).
- For adaptive methods, `times` could be irregularly spaced.
- No `bin_width` field — that's a property of the `BinningParams`, not the output.

### Per-Trial Data (for Uncertainty and Z-Score)

```cpp
/// Optional per-trial breakdown, computed by estimators that support it.
/// Each row is one trial's rate curve, evaluated at the same `times` as the
/// aggregate RateEstimate.
struct PerTrialData {
    /// trials × time_points matrix (row-major: per_trial_values[trial][time_idx])
    std::vector<std::vector<double>> per_trial_values;
};

/// Extended output when per-trial data is requested.
struct RateEstimateWithTrials {
    RateEstimate estimate;                ///< Mean/aggregate estimate
    PerTrialData trials;                  ///< Per-trial breakdown
};
```

**Key insight**: Z-score, SEM, and confidence intervals all require knowing the distribution *across trials*. Currently, z-score is computed across *time bins within a unit* (which is a within-unit normalization). If you also want *across-trial* z-score or CI, you need the per-trial matrix. Having `PerTrialData` optionally available from the estimator is cleaner than trying to reconstruct it after the fact.

Whether you compute z-scores across time bins (current behavior) or across trials is a separate question — both are valid, and they answer different questions. The current within-unit z-score answers "which time bins have unusually high/low rates for this unit?" An across-trial z-score at each time bin would answer "how many standard deviations is the mean rate at this time bin from the overall mean across trials?" The per-trial data supports both.

**Decision**: Within-unit z-score only for now. Population-level z-score (across all units) can be added later if needed.

### Estimation Methods

Keep the `std::variant` dispatch — it's lightweight and appropriate here. But each method should produce the same `RateEstimate` output type:

```cpp
// ── Parameter structs ──────────────────────────────────────────

struct BinningParams {
    double bin_size = 10.0;
};

struct GaussianKernelParams {
    double sigma = 20.0;            ///< Kernel bandwidth (same units as window)
    double eval_step = 1.0;         ///< Spacing of evaluation points
};

struct CausalExponentialParams {
    double tau = 50.0;              ///< Decay time constant
    double eval_step = 1.0;
};

using EstimationParams = std::variant<BinningParams, GaussianKernelParams, CausalExponentialParams>;

// ── Estimation functions ───────────────────────────────────────

/// Single-unit estimation (aggregate across trials)
[[nodiscard]] RateEstimate estimateRate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params = BinningParams{});

/// Single-unit with per-trial breakdown
[[nodiscard]] RateEstimateWithTrials estimateRateWithTrials(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params = BinningParams{});

/// Multi-unit (heatmap use case)
[[nodiscard]] std::vector<RateEstimate> estimateRates(
        std::vector<UnitGatherContext> const & units,
        double window_size,
        EstimationParams const & params = BinningParams{});
```

For binning, `estimateRateWithTrials` would return per-trial histograms (each trial's individual bin counts). For kernel smoothing, each trial's individual smoothed curve. The aggregate `estimate.values` is always the trial-averaged result.

### Normalization as Composable Transforms

Instead of a monolithic `normalizeRateProfiles()` that knows about `RateProfile` internals, normalization functions should operate on `std::span<double>`:

```cpp
// ── Normalization primitives (operate on value vectors in-place) ─

/// Convert raw counts to firing rate: values[i] /= (num_trials × bin_width_s)
void toFiringRateHz(std::span<double> values, size_t num_trials,
                    double sample_spacing, double time_units_per_second);

/// Convert raw counts to count-per-trial: values[i] /= num_trials
void toCountPerTrial(std::span<double> values, size_t num_trials);

/// Per-unit z-score: values[i] = (values[i] - mean) / std
void zScoreNormalize(std::span<double> values);

/// Per-unit min-max to [0, 1]
void minMaxNormalize(std::span<double> values);
```

These are simple, testable, composable functions. A caller can chain them:

```cpp
auto est = estimateRate(gathered, tf, window_size, BinningParams{.bin_size = 10.0});
toCountPerTrial(est.values, est.num_trials);     // raw counts → count/trial
zScoreNormalize(est.values);                      // count/trial → z-score
```

For the heatmap widget's combo box, you'd still have a convenience enum + dispatch function, but it would delegate to these primitives:

```cpp
/// Convenience: apply a named scaling to a RateEstimate in-place
enum class ScalingMode {
    FiringRateHz,
    ZScore,
    Normalized01,
    RawCount,
    CountPerTrial,
};

/// Apply scaling in-place to a single RateEstimate.
/// sample_spacing is read from estimate.metadata.
void applyScaling(RateEstimate & estimate,
                  ScalingMode mode,
                  double time_units_per_second);

/// Apply scaling consistently to aggregate + all per-trial curves.
/// For z-score: uses aggregate mean/std so CI bounds are in the same space.
void applyScaling(RateEstimateWithTrials & data,
                  ScalingMode mode,
                  double time_units_per_second);
```

This replaces `HeatmapScaling` and `normalizeRateProfiles()` with something reusable by both PSTH and Heatmap.

### Uncertainty Quantification

With `PerTrialData` available, confidence intervals become straightforward:

```cpp
struct ConfidenceBand {
    std::vector<double> lower;    ///< Lower bound at each time point
    std::vector<double> upper;    ///< Upper bound at each time point
};

/// Compute SEM-based confidence band: mean ± k × SEM
[[nodiscard]] ConfidenceBand computeSEM(
        RateEstimateWithTrials const & data,
        double k = 1.0);           ///< k=1.96 for 95% CI

/// Compute percentile-based confidence band
[[nodiscard]] ConfidenceBand computePercentileCI(
        RateEstimateWithTrials const & data,
        double lower_pct = 2.5,    ///< e.g. 2.5 for 95% CI
        double upper_pct = 97.5);

/// Bootstrap confidence band (resamples trials)
[[nodiscard]] ConfidenceBand bootstrapCI(
        RateEstimateWithTrials const & data,
        size_t n_resamples = 1000,
        double ci_level = 0.95);
```

**Interaction with normalization**: The `ConfidenceBand` is computed on whatever values are in the `RateEstimateWithTrials`. So the workflow for "z-scored confidence intervals" is:

To avoid inconsistencies, `applyScaling()` has an overload for `RateEstimateWithTrials` that applies the same transform to both aggregate and per-trial data in one call:

```cpp
/// Apply scaling to aggregate + all per-trial curves consistently.
/// For z-score: uses aggregate mean/std to normalize all trials (not per-trial stats).
void applyScaling(RateEstimateWithTrials & data,
                  ScalingMode mode,
                  double time_units_per_second);
```

The z-score path uses the *aggregate* mean and std to normalize each trial, ensuring CI bounds are in the same z-score space as the mean:

```cpp
auto data = estimateRateWithTrials(gathered, tf, window_size, params);
applyScaling(data, ScalingMode::ZScore, 1000.0);  // normalizes aggregate + all trials
auto ci = computeSEM(data);  // CI is in z-score space — consistent
```

Internally, `applyScaling` for `RateEstimateWithTrials` delegates to a helper:

```cpp
/// Z-score each trial using statistics from the aggregate estimate
void zScoreTrialsFromAggregate(RateEstimateWithTrials & data);
```

### Bridging to Rendering

The conversion from `RateEstimate` to rendering types remains the widget's responsibility, but it's now trivial:

```cpp
// In HeatmapOpenGLWidget::rebuildScene():
auto estimates = estimateRates(contexts, window_size, params);

// Apply scaling
for (auto & est : estimates) {
    applyScaling(est, scaling_mode, time_units_per_second);
}

// Convert to HeatmapRowData for the mapper
// times[] are bin centers; the mapper needs left edges and width.
std::vector<CorePlotting::HeatmapRowData> rows;
for (auto & est : estimates) {
    double const spacing = est.metadata.sample_spacing;
    double const left_edge = est.times.front() - spacing / 2.0;
    rows.push_back({std::move(est.values), left_edge, spacing});
}
```

For the PSTH (which plots bars or lines), the conversion is:
```cpp
auto est = estimateRate(gathered, tf, window_size, params);
applyScaling(est, mode, tps);
// est.times and est.values are directly plottable as a line or bar chart
```

### `sample_spacing` in Metadata

The Hz conversion (`toFiringRateHz`) needs to know the sample spacing (bin width for binning, eval_step for kernels) to divide by time. This is stored in `RateEstimate::Metadata` so the output is self-describing and callers don't need to thread `EstimationParams` through the normalization layer:

```cpp
struct RateEstimate {
    std::vector<double> times;
    std::vector<double> values;
    size_t num_trials = 0;

    /// Metadata about how this estimate was produced
    struct Metadata {
        double sample_spacing = 1.0;  ///< Time between evaluation points
                                       ///  (bin_size for binning, eval_step for kernels)
    };
    Metadata metadata;
};
```

The estimator fills `metadata.sample_spacing` automatically. `applyScaling()` reads it from the estimate rather than requiring an extra parameter.

---

## Summary of Proposed API

```
Files:
  EventRateEstimation/
  ├── RateEstimate.hpp          # RateEstimate, PerTrialData, RateEstimateWithTrials, ScalingMode
  ├── RateEstimation.hpp        # EstimationParams, estimateRate, estimateRates, estimateRateWithTrials
  ├── RateNormalization.hpp     # toFiringRateHz, toCountPerTrial, zScoreNormalize, minMaxNormalize,
  │                             # applyScaling (single + with-trials), zScoreTrialsFromAggregate
  ├── RateUncertainty.hpp       # ConfidenceBand, computeSEM, computePercentileCI, bootstrapCI
  ├── GatherContext.hpp         # UnitGatherContext, createUnitGatherContext[s] (unchanged)
  └── RateEstimation.cpp        # all implementations
```

### Migration Checklist

| Current | Proposed | Notes |
|---------|----------|-------|
| `RateProfile` | `RateEstimate` | `times` + `values` instead of `counts` + `bin_start` + `bin_width` |
| `NormalizedRow` | removed | `RateEstimate` serves this role directly after in-place normalization |
| `HeatmapScaling` | `ScalingMode` | Renamed, lives in `RateEstimate.hpp` (not heatmap-specific) |
| `normalizeRateProfiles()` | `applyScaling()` (in-place, two overloads) | Single `RateEstimate` or `RateEstimateWithTrials` |
| `estimateRate()` returns `RateProfile` | Returns `RateEstimate` | Same function signature otherwise |
| (not possible) | `estimateRateWithTrials()` | New: enables CI/variability |
| (not possible) | `ConfidenceBand` | New: returned by CI functions |

### Resolved Design Decisions

1. **Bin centers in `times`**: `times[]` always reports bin centers for binned estimation. Consumers that need rectangle geometry (heatmap mapper) reconstruct left edges via `times[i] - metadata.sample_spacing / 2`. Line-plot consumers use the centers directly.

2. **Within-unit z-score only**: Z-score normalizes each unit independently across its own time bins. Population-level z-score (across all units) is deferred to a future phase if needed.

3. **Opt-in per-trial data**: `estimateRateWithTrials()` is a separate function. The default `estimateRate()` does not allocate per-trial storage, keeping the common path lightweight.

4. **Consistent normalization via `applyScaling(RateEstimateWithTrials &)`**: An overload of `applyScaling` handles both aggregate and per-trial data in one call. For z-score, it uses the *aggregate* mean/std to normalize all trials, ensuring CI bounds computed afterward are in the same z-score space as the mean.

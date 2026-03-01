#ifndef EVENT_RATE_ESTIMATION_HPP
#define EVENT_RATE_ESTIMATION_HPP

/**
 * @file EventRateEstimation.hpp
 * @brief Shared infrastructure for gathering and estimating firing rates from DigitalEventSeries
 *
 * This library provides two complementary services:
 *
 * ## 1. Gather Context Creation
 *
 * Wraps `PlotAlignmentGather` to produce `UnitGatherContext` objects that bundle
 * a `GatherResult<DigitalEventSeries>` with its `TimeFrame` and source key for
 * downstream processing. Provides both single-key and batch variants.
 *
 * @code
 * // Single unit
 * auto ctx = createUnitGatherContext(dm, "spikes_unit_1", alignment_data);
 *
 * // Multiple units (for HeatmapWidget)
 * auto ctxs = createUnitGatherContexts(dm, {"unit_1", "unit_2", "unit_3"}, alignment_data);
 * @endcode
 *
 * ## 2. Rate Estimation
 *
 * Converts aligned trial data into `RateEstimate` objects — paired `(times, values)`
 * vectors suitable for direct plotting. The estimation method is a variation point
 * expressed via `EstimationParams`, a `std::variant` of per-method parameter structs.
 * Currently only `BinningParams` (histogram binning) is implemented; additional
 * methods are added by:
 *  - Defining a new `*Params` struct below
 *  - Adding it to the `EstimationParams` alias
 *  - Adding a visitor branch in `EventRateEstimation.cpp`
 *
 * @code
 * // Single unit — default binning
 * auto est = estimateRate(ctx.gathered, ctx.time_frame.get(), window_ms);
 *
 * // Single unit — custom bin size
 * auto est = estimateRate(ctx.gathered, ctx.time_frame.get(), window_ms,
 *                         BinningParams{.bin_size = 5.0});
 *
 * // Single unit with per-trial data (for CI/variability)
 * auto data = estimateRateWithTrials(ctx.gathered, ctx.time_frame.get(), window_ms);
 *
 * // Multiple units — heatmap use case
 * auto estimates = estimateRates(contexts, window_ms);
 * @endcode
 *
 * ## Design notes
 *
 * - **No Qt dependency**: this library is pure C++23 and can be used from any
 *   rendering backend.
 * - **Raw counts by default**: `RateEstimate::values` stores raw event counts.
 *   Use `applyScaling()` from `RateNormalization.hpp` to convert to Hz, z-score, etc.
 * - **`std::variant` variation point**: runtime method selection with zero heap
 *   allocation overhead (versus a virtual base class strategy). New methods are
 *   backward-compatible additions to the variant.
 * - **Method-agnostic output**: All estimation methods produce the same `RateEstimate`
 *   output type with paired `times[]` + `values[]`.
 *
 * @see RateEstimate.hpp for the output types (RateEstimate, RateEstimateWithTrials)
 * @see RateNormalization.hpp for scaling/normalization transforms (applyScaling)
 * @see RateUncertainty.hpp for confidence band computation
 * @see Plots/Common/PlotAlignmentGather.hpp for the underlying gather functions
 * @see PSTHWidget — primary consumer (single-unit histogram)
 * @see HeatmapWidget — primary consumer (multi-unit rate matrix)
 */

#include "RateEstimate.hpp"
#include "RateNormalization.hpp"
#include "EstimationParams.hpp"

#include "DataManager/DataManager.hpp"
#include "GatherResult/GatherResult.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Gather Context
// =============================================================================

/**
 * @brief Bundles a GatherResult with its TimeFrame and source key
 *
 * Widgets require both the trial-aligned views (`gathered`) and the source
 * `TimeFrame` to convert `TimeFrameIndex` event times to absolute times for
 * bin assignment. `UnitGatherContext` keeps these together so callers do not
 * need to re-fetch the `TimeFrame` at rate estimation time.
 */
struct UnitGatherContext {
    std::string key;                                    ///< DataManager key of the source series
    GatherResult<DigitalEventSeries> gathered;          ///< Trial-aligned views of the series
    std::shared_ptr<TimeFrame const> time_frame;        ///< TimeFrame for index→time conversion
};

// =============================================================================
// Gathering functions
// =============================================================================

/**
 * @brief Create a UnitGatherContext for a single DigitalEventSeries key
 *
 * Calls `createAlignedGatherResult<DigitalEventSeries>` and also retrieves the
 * source `TimeFrame`. Returns `std::nullopt` when:
 *  - `data_manager` is null or `event_key` is empty
 *  - The gather result is empty (alignment key not found or no events in window)
 *  - The source series does not have a `TimeFrame`
 *
 * @param data_manager DataManager containing the event and alignment series
 * @param event_key    DataManager key of the `DigitalEventSeries` to gather
 * @param alignment_data Alignment configuration (alignment key, window size, etc.)
 * @return Populated `UnitGatherContext`, or `std::nullopt` on failure
 */
[[nodiscard]] std::optional<UnitGatherContext> createUnitGatherContext(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & event_key,
        PlotAlignmentData const & alignment_data);

/**
 * @brief Create UnitGatherContexts for a batch of DigitalEventSeries keys
 *
 * Applies `createUnitGatherContext` to each key in `event_keys` with the
 * same alignment settings. Keys for which gathering fails are silently skipped;
 * the returned vector may therefore be shorter than `event_keys`.
 *
 * Order is preserved: the i-th element of the result corresponds to the i-th
 * key that succeeded.
 *
 * @param data_manager DataManager containing the event and alignment series
 * @param event_keys   DataManager keys of the `DigitalEventSeries` to gather
 * @param alignment_data Alignment configuration shared across all units
 * @return Vector of successfully gathered `UnitGatherContext` objects
 */
[[nodiscard]] std::vector<UnitGatherContext> createUnitGatherContexts(
        std::shared_ptr<DataManager> const & data_manager,
        std::vector<std::string> const & event_keys,
        PlotAlignmentData const & alignment_data);

// =============================================================================
// Rate Estimation: parameter structs and variation point
// =============================================================================

// Parameter structs (BinningParams, GaussianKernelParams, CausalExponentialParams)
// and the EstimationParams variant are defined in EstimationParams.hpp for
// lightweight inclusion by state headers that don't need the full gathering API.

// =============================================================================
// Single-unit estimation
// =============================================================================

/**
 * @brief Estimate the rate for a single unit (aggregate across trials)
 *
 * Returns a `RateEstimate` with paired `times[]` (bin centers for binning)
 * and `values[]` (raw event counts summed across all trials).
 *
 * Uses `time_frame` to convert each event's `TimeFrameIndex` to an absolute
 * time before computing the relative time with respect to the per-trial
 * alignment time from `gathered`.
 *
 * If `time_frame` is null, `TimeFrameIndex::getValue()` is used directly as
 * the absolute time (identity mapping).
 *
 * @param gathered    Trial-aligned views of one `DigitalEventSeries`
 * @param time_frame  TimeFrame of the source series (may be null)
 * @param window_size Total window span in time units (bins cover ±window_size/2)
 * @param params      Estimation method and parameters (default: `BinningParams{}`)
 * @return `RateEstimate` with times, values, num_trials, and metadata
 */
[[nodiscard]] RateEstimate estimateRate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params = BinningParams{});

/**
 * @brief Estimate rate with per-trial breakdown for a single unit
 *
 * Same as `estimateRate()` but additionally returns per-trial rate curves
 * in `RateEstimateWithTrials::trials`. The aggregate `estimate` is the
 * trial-summed result (same as `estimateRate()`), and each trial's individual
 * histogram is stored separately.
 *
 * Use this when you need uncertainty quantification (SEM, CI, bootstrap)
 * via the functions in `RateUncertainty.hpp`.
 *
 * @param gathered    Trial-aligned views of one `DigitalEventSeries`
 * @param time_frame  TimeFrame of the source series (may be null)
 * @param window_size Total window span in time units
 * @param params      Estimation method and parameters (default: `BinningParams{}`)
 * @return `RateEstimateWithTrials` with aggregate + per-trial data
 */
[[nodiscard]] RateEstimateWithTrials estimateRateWithTrials(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params = BinningParams{});

// =============================================================================
// Multi-unit estimation (heatmap use case)
// =============================================================================

/**
 * @brief Estimate rates for multiple units
 *
 * Applies `estimateRate` independently to each `UnitGatherContext` in `units`.
 * All units share the same `window_size` and `params`. The output vector is
 * in the same order as the input and has the same length.
 *
 * This is the primary entry point for `HeatmapWidget`, which needs a
 * `units × time_bins` rate matrix.
 *
 * @param units       Per-unit gather contexts (from `createUnitGatherContexts`)
 * @param window_size Total window span in time units
 * @param params      Estimation method and parameters (default: `BinningParams{}`)
 * @return One `RateEstimate` per unit, in input order
 */
[[nodiscard]] std::vector<RateEstimate> estimateRates(
        std::vector<UnitGatherContext> const & units,
        double window_size,
        EstimationParams const & params = BinningParams{});

} // namespace WhiskerToolbox::Plots

#endif // EVENT_RATE_ESTIMATION_HPP

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
 * Converts aligned trial data into per-bin event count profiles. The estimation
 * method is a variation point expressed via `EstimationParams`, a `std::variant`
 * of per-method parameter structs. Currently only `BinningParams` (histogram
 * binning) is implemented; additional methods are added by:
 *  - Defining a new `*Params` struct below
 *  - Adding it to the `EstimationParams` alias
 *  - Adding a visitor branch in `EventRateEstimation.cpp`
 *
 * @code
 * // Single unit — default binning
 * auto profile = estimateRate(ctx.gathered, ctx.time_frame.get(), window_ms);
 *
 * // Single unit — custom bin size
 * auto profile = estimateRate(ctx.gathered, ctx.time_frame.get(), window_ms,
 *                             BinningParams{.bin_size = 5.0});
 *
 * // Multiple units — heatmap use case
 * auto profiles = estimateRates(contexts, window_ms);
 * @endcode
 *
 * ## Design notes
 *
 * - **No Qt dependency**: this library is pure C++23 and can be used from any
 *   rendering backend.
 * - **Raw counts, not rates**: `RateProfile::counts` stores raw event counts per
 *   bin. Callers choose their own normalisation (÷ num_trials for mean count per
 *   trial, ÷ (num_trials × bin_size_s) for Hz, etc.).
 * - **`std::variant` variation point**: runtime method selection with zero heap
 *   allocation overhead (versus a virtual base class strategy). New methods are
 *   backward-compatible additions to the variant.
 *
 * @see Plots/Common/PlotAlignmentGather.hpp for the underlying gather functions
 * @see PSTHWidget — primary consumer (single-unit histogram)
 * @see HeatmapWidget — primary consumer (multi-unit rate matrix)
 */

#include "RateScaling.hpp"

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
// Rate Estimation: parameter structs (one per method)
// =============================================================================

/**
 * @brief Parameters for simple histogram binning
 *
 * Divides the half-open window `[-window_size/2, +window_size/2)` into
 * equal-width bins of `bin_size` and counts events that fall into each bin
 * across all trials.
 *
 * The result is a raw count histogram. To obtain:
 *  - **Mean count/trial**: divide `counts[i]` by `num_trials`
 *  - **Firing rate (Hz)**: divide by `num_trials × (bin_size / time_units_per_s)`
 */
struct BinningParams {
    double bin_size = 10.0; ///< Bin width in the same time units as `window_size`
};

// Future methods follow the same pattern — define a struct, add to the variant:
//
//   struct GaussianKernelParams { double sigma = 20.0; };
//   struct CausalExponentialParams { double tau = 50.0; };

// =============================================================================
// Rate Estimation: variation point
// =============================================================================

/**
 * @brief Tagged union of all supported rate estimation method parameter structs
 *
 * This is the single variation point for estimation method selection.
 * Selecting a method at runtime is zero-overhead compared to a virtual
 * base-class strategy — `std::visit` dispatches via a jump table.
 *
 * To add a new method:
 *  1. Define a new `*Params` struct above
 *  2. Append it to this alias: `using EstimationParams = std::variant<BinningParams, NewParams>`
 *  3. Add a `if constexpr (std::is_same_v<T, NewParams>)` branch in
 *     `EventRateEstimation.cpp`
 */
using EstimationParams = std::variant<BinningParams>;

// =============================================================================
// Output
// =============================================================================

/**
 * @brief Rate estimation result for a single unit
 *
 * Stores raw per-bin event counts and enough metadata to reconstruct the
 * time axis and apply normalisation.  The bin at index `i` covers the
 * half-open interval `[bin_start + i*bin_width, bin_start + (i+1)*bin_width)`.
 */
struct RateProfile {
    std::vector<double> counts; ///< Raw event count per bin (summed across all trials)
    double bin_start;           ///< Left edge of the first bin (relative to t=0 alignment)
    double bin_width;           ///< Width of each bin (same units as `window_size`)
    size_t num_trials;          ///< Number of trials that contributed (null trials excluded)
};

// =============================================================================
// Single-unit estimation
// =============================================================================

/**
 * @brief Estimate the rate profile for a single unit
 *
 * Bins all events across all trials in `gathered` into a histogram covering
 * `[-window_size/2, +window_size/2)`. Uses `time_frame` to convert each
 * event's `TimeFrameIndex` to an absolute time before computing the relative
 * time with respect to the per-trial alignment time from `gathered`.
 *
 * If `time_frame` is null, `TimeFrameIndex::getValue()` is used directly as
 * the absolute time (identity mapping).
 *
 * @param gathered    Trial-aligned views of one `DigitalEventSeries`
 * @param time_frame  TimeFrame of the source series (may be null)
 * @param window_size Total window span in time units (bins cover ±window_size/2)
 * @param params      Estimation method and parameters (default: `BinningParams{}`)
 * @return `RateProfile` with per-bin counts and axis metadata
 */
[[nodiscard]] RateProfile estimateRate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params = BinningParams{});

// =============================================================================
// Multi-unit estimation (heatmap use case)
// =============================================================================

/**
 * @brief Estimate rate profiles for multiple units
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
 * @return One `RateProfile` per unit, in input order
 */
[[nodiscard]] std::vector<RateProfile> estimateRates(
        std::vector<UnitGatherContext> const & units,
        double window_size,
        EstimationParams const & params = BinningParams{});

// =============================================================================
// Scaling / Normalization (types in RateScaling.hpp)
// =============================================================================

/**
 * @brief Apply a scaling/normalization transform to a set of rate profiles
 *
 * Converts raw `RateProfile` objects (with per-bin event counts) into
 * `NormalizedRow` objects whose values reflect the chosen scaling mode.
 *
 * The `time_units_per_second` parameter is required for `FiringRate` scaling
 * to convert bin widths from the data's native time units into seconds.
 * For example, if time is in milliseconds, pass 1000.0.
 *
 * | Scaling mode    | Formula per bin                                  |
 * |-----------------|--------------------------------------------------|
 * | FiringRate      | count / (bin_size_s × num_trials)                |
 * | CountPerTrial   | count / num_trials                               |
 * | RawCount        | count (no transform)                             |
 * | Normalized01    | per-unit (v - min) / (max - min)                 |
 * | ZScore          | per-unit (v - mean) / std                        |
 *
 * @param profiles           Rate profiles from `estimateRate[s]()`
 * @param scaling            Scaling mode to apply
 * @param time_units_per_second Conversion factor from data time units to seconds
 *                              (only used by FiringRate; ignored by others)
 * @return One `NormalizedRow` per profile, in the same order
 */
[[nodiscard]] std::vector<NormalizedRow> normalizeRateProfiles(
        std::vector<RateProfile> const & profiles,
        HeatmapScaling scaling,
        double time_units_per_second = 1.0);

} // namespace WhiskerToolbox::Plots

#endif // EVENT_RATE_ESTIMATION_HPP

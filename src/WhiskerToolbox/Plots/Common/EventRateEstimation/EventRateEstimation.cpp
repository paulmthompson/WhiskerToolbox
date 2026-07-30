#include "EventRateEstimation.hpp"

#include "GatherResult/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentWindowPreparation.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Neuralyzer::Plots {

namespace {

/**
 * @brief Core gather dispatch using PlotAlignmentData (no Qt dependency)
 */
[[nodiscard]] GatherResult<DigitalEventSeries> gatherFromAlignmentData(
        std::shared_ptr<DataManager> const & data_manager,
        std::shared_ptr<DigitalEventSeries> const & source,
        PlotAlignmentData const & alignment_data) {
    if (!data_manager || !source || alignment_data.alignment_event_key.empty()) {
        return GatherResult<DigitalEventSeries>{};
    }

    auto prepared = prepareAlignmentWindows(
            data_manager,
            alignment_data,
            source->getTimeFrame());
    if (!prepared.isValid()) {
        return GatherResult<DigitalEventSeries>{};
    }

    return gather(source, prepared.windows, prepared.alignment_points);
}

}// anonymous namespace

// =============================================================================
// Gathering
// =============================================================================

std::optional<UnitGatherContext> createUnitGatherContext(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & event_key,
        PlotAlignmentData const & alignment_data) {
    if (!data_manager || event_key.empty()) {
        return std::nullopt;
    }

    auto source = data_manager->getData<DigitalEventSeries>(event_key);
    if (!source) {
        return std::nullopt;
    }

    auto tf = source->getTimeFrame();
    if (!tf) {
        return std::nullopt;
    }

    auto gathered = gatherFromAlignmentData(data_manager, source, alignment_data);
    if (gathered.empty()) {
        return std::nullopt;
    }

    return UnitGatherContext{event_key, std::move(gathered), std::move(tf)};
}

std::vector<UnitGatherContext> createUnitGatherContexts(
        std::shared_ptr<DataManager> const & data_manager,
        std::vector<std::string> const & event_keys,
        PlotAlignmentData const & alignment_data) {
    std::vector<UnitGatherContext> result;
    result.reserve(event_keys.size());

    for (auto const & key: event_keys) {
        auto ctx = createUnitGatherContext(data_manager, key, alignment_data);
        if (ctx.has_value()) {
            result.push_back(std::move(*ctx));
        }
    }

    return result;
}

// =============================================================================
// Rate estimation: internal per-method workers
// =============================================================================

namespace {

/**
 * @brief Build the bin-center time vector for a binned histogram
 *
 * @param num_bins     Number of bins
 * @param half_window  Half the analysis window
 * @param bin_size     Width of each bin
 * @return Vector of bin centers
 */
[[nodiscard]] std::vector<double> buildBinCenters(
        int num_bins, double half_window, double bin_size) {
    std::vector<double> times(static_cast<size_t>(num_bins));
    for (int i = 0; i < num_bins; ++i) {
        // Left edge of bin i: -half_window + i * bin_size
        // Center: left_edge + bin_size / 2
        times[static_cast<size_t>(i)] =
                -half_window + static_cast<double>(i) * bin_size + bin_size / 2.0;
    }
    return times;
}

/**
 * @brief Histogram binning implementation (aggregate across trials)
 *
 * Iterates over every valid trial in `gathered`, converts each event's
 * `TimeFrameIndex` to an absolute time via `time_frame` (or falls back to the
 * raw index value when `time_frame` is null), subtracts the per-trial alignment
 * time to get a relative time, and increments the corresponding bin.
 *
 * Events outside `[-half_window, +half_window)` are silently discarded.
 *
 * Returns a `RateEstimate` with bin centers in `times[]` and raw counts in
 * `values[]`.
 */
[[nodiscard]] RateEstimate binningEstimate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        BinningParams const & params) {
    double const half_window = window_size / 2.0;
    double const bin_size = params.bin_size;

    if (bin_size <= 0.0 || window_size <= 0.0) {
        return RateEstimate{};
    }

    int const num_bins = static_cast<int>(std::ceil(window_size / bin_size));
    if (num_bins <= 0) {
        return RateEstimate{};
    }

    std::vector<double> histogram(static_cast<size_t>(num_bins), 0.0);
    size_t num_trials = 0;

    for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
        auto const & trial_view = gathered[trial_idx];
        if (!trial_view) {
            continue;
        }

        // t=0 reference for this trial (absolute time)
        auto const alignment_time =
                static_cast<double>(gathered.alignmentTimeAt(trial_idx).getValue());

        if (!time_frame) {
            std::throw_with_nested(std::runtime_error("binningEstimate: no TimeFrame"));
        }
        for (auto const & event: trial_view->view()) {
            double const event_abs = static_cast<double>(event.time().getValue());

            double const relative_time = event_abs - alignment_time;

            // Discard events outside the analysis window.
            if (relative_time < -half_window || relative_time >= half_window) {
                continue;
            }

            // Bin 0 covers [-half_window, -half_window + bin_size).
            int bin_index = static_cast<int>(
                    std::floor((relative_time + half_window) / bin_size));
            bin_index = std::clamp(bin_index, 0, num_bins - 1);

            histogram[static_cast<size_t>(bin_index)] += 1.0;
        }

        ++num_trials;
    }

    RateEstimate result;
    result.times = buildBinCenters(num_bins, half_window, bin_size);
    result.values = std::move(histogram);
    result.num_trials = num_trials;
    result.metadata.sample_spacing = bin_size;
    return result;
}

/**
 * @brief Histogram binning with per-trial breakdown
 *
 * Same as `binningEstimate()` but also stores each trial's individual histogram.
 */
[[nodiscard]] RateEstimateWithTrials binningEstimateWithTrials(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        BinningParams const & params) {
    double const half_window = window_size / 2.0;
    double const bin_size = params.bin_size;

    if (bin_size <= 0.0 || window_size <= 0.0) {
        return RateEstimateWithTrials{};
    }

    int const num_bins = static_cast<int>(std::ceil(window_size / bin_size));
    if (num_bins <= 0) {
        return RateEstimateWithTrials{};
    }

    auto const n_bins = static_cast<size_t>(num_bins);
    std::vector<double> aggregate(n_bins, 0.0);
    std::vector<std::vector<double>> per_trial;
    size_t num_trials = 0;

    for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
        auto const & trial_view = gathered[trial_idx];
        if (!trial_view) {
            continue;
        }

        std::vector<double> trial_hist(n_bins, 0.0);

        auto const alignment_time =
                static_cast<double>(gathered.alignmentTimeAt(trial_idx).getValue());

        if (!time_frame) {
            std::throw_with_nested(std::runtime_error("binningEstimateWithTrials: no TimeFrame"));
        }

        for (auto const & event: trial_view->view()) {
            double const event_abs = static_cast<double>(event.time().getValue());

            double const relative_time = event_abs - alignment_time;

            if (relative_time < -half_window || relative_time >= half_window) {
                continue;
            }

            int bin_index = static_cast<int>(
                    std::floor((relative_time + half_window) / bin_size));
            bin_index = std::clamp(bin_index, 0, num_bins - 1);

            trial_hist[static_cast<size_t>(bin_index)] += 1.0;
            aggregate[static_cast<size_t>(bin_index)] += 1.0;
        }

        per_trial.push_back(std::move(trial_hist));
        ++num_trials;
    }

    RateEstimateWithTrials result;
    result.estimate.times = buildBinCenters(num_bins, half_window, bin_size);
    result.estimate.values = std::move(aggregate);
    result.estimate.num_trials = num_trials;
    result.estimate.metadata.sample_spacing = bin_size;
    result.trials.per_trial_values = std::move(per_trial);
    return result;
}

}// anonymous namespace

// =============================================================================
// Public dispatch functions
// =============================================================================

RateEstimate estimateRate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params) {
    return std::visit(
            [&](auto const & p) -> RateEstimate {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BinningParams>) {
                    return binningEstimate(gathered, time_frame, window_size, p);
                }
                // Stub for unimplemented methods
                return RateEstimate{};
            },
            params);
}

RateEstimateWithTrials estimateRateWithTrials(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params) {
    return std::visit(
            [&](auto const & p) -> RateEstimateWithTrials {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BinningParams>) {
                    return binningEstimateWithTrials(
                            gathered, time_frame, window_size, p);
                }
                // Stub for unimplemented methods
                return RateEstimateWithTrials{};
            },
            params);
}

std::vector<RateEstimate> estimateRates(
        std::vector<UnitGatherContext> const & units,
        double window_size,
        EstimationParams const & params) {
    std::vector<RateEstimate> results;
    results.reserve(units.size());

    for (auto const & unit: units) {
        results.push_back(estimateRate(
                unit.gathered, unit.time_frame.get(), window_size, params));
    }

    return results;
}

}// namespace Neuralyzer::Plots

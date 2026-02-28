#include "EventRateEstimation.hpp"

// We do NOT include PlotAlignmentGather.hpp here because it transitively
// includes PlotAlignmentState.hpp (a QObject subclass), which would introduce
// a Qt dependency into this otherwise Qt-free library.  Instead we replicate
// only the gather logic that depends on PlotAlignmentData (a plain POD struct).

#include "GatherResult/GatherResult.hpp"
#include "TransformsV2/extension/IntervalAdapters.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Internal gather helpers (subset of PlotAlignmentGather.hpp — no Qt needed)
// =============================================================================

namespace {

using WhiskerToolbox::Transforms::V2::AlignmentPoint;
using WhiskerToolbox::Transforms::V2::expandEvents;
using WhiskerToolbox::Transforms::V2::withAlignment;

[[nodiscard]] AlignmentPoint toAlignmentPointLocal(IntervalAlignmentType type) noexcept
{
    return (type == IntervalAlignmentType::End)
            ? AlignmentPoint::End
            : AlignmentPoint::Start;
}

/**
 * @brief Create a GatherResult aligned to a DigitalEventSeries with window expansion
 */
[[nodiscard]] GatherResult<DigitalEventSeries> gatherWithEventSeries(
        std::shared_ptr<DigitalEventSeries> const & source,
        std::shared_ptr<DigitalEventSeries> const & alignment_events,
        double half_window)
{
    if (!source || !alignment_events) {
        return GatherResult<DigitalEventSeries>{};
    }
    auto adapter = expandEvents(
            alignment_events,
            static_cast<int64_t>(half_window),
            static_cast<int64_t>(half_window));
    return gather(source, adapter);
}

/**
 * @brief Create a GatherResult aligned to a DigitalIntervalSeries with window expansion
 */
[[nodiscard]] GatherResult<DigitalEventSeries> gatherWithIntervalSeries(
        std::shared_ptr<DigitalEventSeries> const & source,
        std::shared_ptr<DigitalIntervalSeries> const & alignment_intervals,
        AlignmentPoint align,
        double half_window)
{
    if (!source || !alignment_intervals) {
        return GatherResult<DigitalEventSeries>{};
    }
    auto adapter = withAlignment(
            alignment_intervals,
            align,
            static_cast<int64_t>(half_window),
            static_cast<int64_t>(half_window));
    return gather(source, adapter);
}

/**
 * @brief Core gather dispatch using PlotAlignmentData (no Qt dependency)
 */
[[nodiscard]] GatherResult<DigitalEventSeries> gatherFromAlignmentData(
        std::shared_ptr<DataManager> const & data_manager,
        std::shared_ptr<DigitalEventSeries> const & source,
        PlotAlignmentData const & alignment_data)
{
    if (!data_manager || !source || alignment_data.alignment_event_key.empty()) {
        return GatherResult<DigitalEventSeries>{};
    }

    double const half_window = alignment_data.window_size / 2.0;
    DM_DataType const type =
            data_manager->getType(alignment_data.alignment_event_key);

    if (type == DM_DataType::DigitalEvent) {
        auto alignment_events =
                data_manager->getData<DigitalEventSeries>(
                        alignment_data.alignment_event_key);
        if (!alignment_events) {
            return GatherResult<DigitalEventSeries>{};
        }
        return gatherWithEventSeries(source, alignment_events, half_window);

    } else if (type == DM_DataType::DigitalInterval) {
        auto alignment_intervals =
                data_manager->getData<DigitalIntervalSeries>(
                        alignment_data.alignment_event_key);
        if (!alignment_intervals) {
            return GatherResult<DigitalEventSeries>{};
        }
        AlignmentPoint const align =
                toAlignmentPointLocal(alignment_data.interval_alignment_type);
        return gatherWithIntervalSeries(
                source, alignment_intervals, align, half_window);
    }

    return GatherResult<DigitalEventSeries>{};
}

} // anonymous namespace

// =============================================================================
// Gathering
// =============================================================================

std::optional<UnitGatherContext> createUnitGatherContext(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & event_key,
        PlotAlignmentData const & alignment_data)
{
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
        PlotAlignmentData const & alignment_data)
{
    std::vector<UnitGatherContext> result;
    result.reserve(event_keys.size());

    for (auto const & key : event_keys) {
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
 * @brief Histogram binning implementation
 *
 * Iterates over every valid trial in `gathered`, converts each event's
 * `TimeFrameIndex` to an absolute time via `time_frame` (or falls back to the
 * raw index value when `time_frame` is null), subtracts the per-trial alignment
 * time to get a relative time, and increments the corresponding bin.
 *
 * Events outside `[-half_window, +half_window)` are silently discarded.
 */
[[nodiscard]] RateProfile binningEstimate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        BinningParams const & params)
{
    double const half_window = window_size / 2.0;
    double const bin_size = params.bin_size;

    if (bin_size <= 0.0 || window_size <= 0.0) {
        return RateProfile{};
    }

    int const num_bins = static_cast<int>(std::ceil(window_size / bin_size));
    if (num_bins <= 0) {
        return RateProfile{};
    }

    std::vector<double> histogram(static_cast<size_t>(num_bins), 0.0);
    size_t num_trials = 0;

    for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
        auto const & trial_view = gathered[trial_idx];
        if (!trial_view) {
            continue;
        }

        // t=0 reference for this trial (absolute time)
        double const alignment_time =
                static_cast<double>(gathered.alignmentTimeAt(trial_idx));

        for (auto const & event : trial_view->view()) {
            double const event_abs = time_frame
                    ? static_cast<double>(
                              time_frame->getTimeAtIndex(event.time()))
                    : static_cast<double>(event.time().getValue());

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

    return RateProfile{std::move(histogram), -half_window, bin_size, num_trials};
}

} // anonymous namespace

// =============================================================================
// Public dispatch functions
// =============================================================================

RateProfile estimateRate(
        GatherResult<DigitalEventSeries> const & gathered,
        TimeFrame const * time_frame,
        double window_size,
        EstimationParams const & params)
{
    return std::visit(
            [&](auto const & p) -> RateProfile {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BinningParams>) {
                    return binningEstimate(gathered, time_frame, window_size, p);
                }
                return RateProfile{};
            },
            params);
}

std::vector<RateProfile> estimateRates(
        std::vector<UnitGatherContext> const & units,
        double window_size,
        EstimationParams const & params)
{
    std::vector<RateProfile> results;
    results.reserve(units.size());

    for (auto const & unit : units) {
        results.push_back(estimateRate(
                unit.gathered, unit.time_frame.get(), window_size, params));
    }

    return results;
}

// =============================================================================
// Normalization
// =============================================================================

namespace {

/**
 * @brief Convert a RateProfile to a NormalizedRow preserving raw counts
 */
[[nodiscard]] NormalizedRow toRawRow(RateProfile const & profile)
{
    return NormalizedRow{profile.counts, profile.bin_start, profile.bin_width};
}

/**
 * @brief Convert a RateProfile to count-per-trial
 */
[[nodiscard]] NormalizedRow toCountPerTrial(RateProfile const & profile)
{
    NormalizedRow row{profile.counts, profile.bin_start, profile.bin_width};
    if (profile.num_trials > 0) {
        double const divisor = static_cast<double>(profile.num_trials);
        for (auto & v : row.values) {
            v /= divisor;
        }
    }
    return row;
}

/**
 * @brief Convert a RateProfile to firing rate (Hz)
 *
 * rate = count / (num_trials × bin_size_seconds)
 */
[[nodiscard]] NormalizedRow toFiringRate(RateProfile const & profile,
                                         double time_units_per_second)
{
    NormalizedRow row{profile.counts, profile.bin_start, profile.bin_width};
    if (profile.num_trials > 0 && profile.bin_width > 0.0) {
        double const bin_size_s = profile.bin_width / time_units_per_second;
        double const divisor = static_cast<double>(profile.num_trials) * bin_size_s;
        for (auto & v : row.values) {
            v /= divisor;
        }
    }
    return row;
}

/**
 * @brief Per-unit z-score normalisation: (value - mean) / std
 *
 * If std is zero (constant signal), all values become 0.
 */
void applyZScore(std::vector<double> & values)
{
    if (values.empty()) {
        return;
    }
    auto const n = static_cast<double>(values.size());
    double const sum = std::accumulate(values.begin(), values.end(), 0.0);
    double const mean = sum / n;

    double sq_sum = 0.0;
    for (double v : values) {
        double const diff = v - mean;
        sq_sum += diff * diff;
    }
    double const std_dev = std::sqrt(sq_sum / n);

    if (std_dev > 0.0) {
        for (auto & v : values) {
            v = (v - mean) / std_dev;
        }
    } else {
        std::fill(values.begin(), values.end(), 0.0);
    }
}

/**
 * @brief Per-unit min-max normalisation to [0, 1]
 *
 * If all values are equal, all become 0.
 */
void applyNormalized01(std::vector<double> & values)
{
    if (values.empty()) {
        return;
    }
    auto const [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    double const vmin = *min_it;
    double const vmax = *max_it;
    double const range = vmax - vmin;

    if (range > 0.0) {
        for (auto & v : values) {
            v = (v - vmin) / range;
        }
    } else {
        std::fill(values.begin(), values.end(), 0.0);
    }
}

} // anonymous namespace

std::vector<NormalizedRow> normalizeRateProfiles(
        std::vector<RateProfile> const & profiles,
        HeatmapScaling scaling,
        double time_units_per_second)
{
    std::vector<NormalizedRow> rows;
    rows.reserve(profiles.size());

    switch (scaling) {
        case HeatmapScaling::RawCount:
            for (auto const & p : profiles) {
                rows.push_back(toRawRow(p));
            }
            break;

        case HeatmapScaling::CountPerTrial:
            for (auto const & p : profiles) {
                rows.push_back(toCountPerTrial(p));
            }
            break;

        case HeatmapScaling::FiringRate:
            for (auto const & p : profiles) {
                rows.push_back(toFiringRate(p, time_units_per_second));
            }
            break;

        case HeatmapScaling::ZScore:
            // First normalise to count-per-trial, then z-score per unit
            for (auto const & p : profiles) {
                auto row = toCountPerTrial(p);
                applyZScore(row.values);
                rows.push_back(std::move(row));
            }
            break;

        case HeatmapScaling::Normalized01:
            // First normalise to count-per-trial, then min-max per unit
            for (auto const & p : profiles) {
                auto row = toCountPerTrial(p);
                applyNormalized01(row.values);
                rows.push_back(std::move(row));
            }
            break;
    }

    return rows;
}

} // namespace WhiskerToolbox::Plots


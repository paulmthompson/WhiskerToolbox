#ifndef NEURALYZER_V2_PRUNE_OVERLAPPING_INTERVALS_HPP
#define NEURALYZER_V2_PRUNE_OVERLAPPING_INTERVALS_HPP

#include <memory>

class DigitalIntervalSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Parameters for overlap pruning of interval series
 *
 * Currently empty; reserved for future pruning policy options.
 *
 * Example JSON:
 * ```json
 * {}
 * ```
 */
struct PruneOverlappingIntervalsParams {};

/**
 * @brief Greedy keep-first pruning of overlapping intervals
 *
 * Container signature: DigitalIntervalSeries -> DigitalIntervalSeries
 *
 * Uses a left-to-right scan: keeps the first interval, then for each subsequent
 * interval checks whether its start overlaps or touches the end of the last kept
 * interval. Overlapping intervals are skipped.
 *
 * The output uses IntervalLayout::Disjoint and preserves the input TimeFrame.
 * Intervals must be sorted by start time (guaranteed by DigitalIntervalSeries).
 *
 * @param input Input interval series (any layout)
 * @param params Pruning parameters (currently unused)
 * @param ctx Compute context for progress/cancellation
 * @return Shared pointer to disjoint DigitalIntervalSeries with non-overlapping intervals
 */
std::shared_ptr<DigitalIntervalSeries> pruneOverlappingIntervals(
        DigitalIntervalSeries const & input,
        PruneOverlappingIntervalsParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_PRUNE_OVERLAPPING_INTERVALS_HPP

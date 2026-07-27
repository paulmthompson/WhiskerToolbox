#ifndef NEURALYZER_V2_INTERVAL_TO_EVENT_HPP
#define NEURALYZER_V2_INTERVAL_TO_EVENT_HPP

#include <rfl.hpp>

#include <memory>

class DigitalEventSeries;
class DigitalIntervalSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Which point within an interval becomes the output event time
 */
enum class IntervalEventPoint {
    start,///< Use interval.start
    end,  ///< Use interval.end
    center///< Use (interval.start + interval.end) / 2
};

/**
 * @brief Parameters for extracting events from intervals
 *
 * Each interval produces one event at the selected point in TimeFrameIndex space.
 *
 * Example JSON:
 * ```json
 * {
 *   "point": "center"
 * }
 * ```
 */
struct IntervalToEventParams {
    IntervalEventPoint point{IntervalEventPoint::start};
};

/**
 * @brief Extract one event per interval from a DigitalIntervalSeries
 *
 * Container signature: DigitalIntervalSeries -> DigitalEventSeries
 *
 * Each interval produces one event at start, end, or center (integer division).
 * The output preserves the input TimeFrame and does not copy source entity IDs.
 *
 * @param input Input interval series
 * @param params Which point within each interval becomes the event time
 * @param ctx Compute context for progress/cancellation
 * @return Shared pointer to DigitalEventSeries with one event per input interval
 */
std::shared_ptr<DigitalEventSeries> intervalToEvent(
        DigitalIntervalSeries const & input,
        IntervalToEventParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_INTERVAL_TO_EVENT_HPP

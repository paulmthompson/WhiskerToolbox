#ifndef NEURALYZER_V2_EVENT_TO_INTERVAL_HPP
#define NEURALYZER_V2_EVENT_TO_INTERVAL_HPP

#include "TimeFrame/TimeFrameIndex.hpp"

#include <memory>

class DigitalEventSeries;
class DigitalIntervalSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Parameters for expanding events into window intervals
 *
 * Each event at index T becomes interval [T - pre_expansion, T + post_expansion]
 * in TimeFrameIndex space.
 *
 * Example JSON:
 * ```json
 * {
 *   "pre_expansion": 10,
 *   "post_expansion": 20
 * }
 * ```
 */
struct EventToIntervalParams {
    TimeFrameIndex pre_expansion = TimeFrameIndex{0};
    TimeFrameIndex post_expansion = TimeFrameIndex{0};
};

/**
 * @brief Expand each event in a DigitalEventSeries into a window interval
 *
 * Container signature: DigitalEventSeries -> DigitalIntervalSeries
 *
 * Each event at TimeFrameIndex T produces one interval:
 * [T - pre_expansion, T + post_expansion]
 *
 * The output uses IntervalLayout::Overlapping and preserves the input TimeFrame.
 *
 * @pre params.pre_expansion and params.post_expansion must be non-negative
 * @param input Input event series
 * @param params Pre/post expansion sizes in TimeFrameIndex units
 * @param ctx Compute context for progress/cancellation
 * @return Shared pointer to overlapping DigitalIntervalSeries
 */
std::shared_ptr<DigitalIntervalSeries> eventToInterval(
        DigitalEventSeries const & input,
        EventToIntervalParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_EVENT_TO_INTERVAL_HPP

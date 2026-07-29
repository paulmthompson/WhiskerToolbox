#ifndef DATAVIEWER_VIEW_WINDOW_HPP
#define DATAVIEWER_VIEW_WINDOW_HPP

/**
 * @file DataViewerViewWindow.hpp
 * @brief View-window time coordinate helpers for DataViewer rendering and hit testing
 */

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "TimeFrame/ClockTicks.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>

class TimeFrame;

namespace DataViewer {

/**
 * @brief Master-clock absolute time at the left edge of the visible view window.
 *
 * @pre When @p master_tf is non-null, @c view_state.x_min is a valid @c TimeFrameIndex into it.
 * @post Returns @c ClockTicks(0) when @p master_tf is null.
 */
[[nodiscard]] inline ClockTicks viewOriginMasterAbsolute(
        std::shared_ptr<TimeFrame> const & master_tf,
        CorePlotting::ViewStateData const & view_state) {
    if (!master_tf) {
        return ClockTicks(0);
    }
    return master_tf->getTimeAtIndex(
            TimeFrameIndex{static_cast<int64_t>(view_state.x_min)});
}

/**
 * @brief Master-clock absolute time at a specific master @c TimeFrameIndex.
 *
 * @pre When @p master_tf is non-null, @p index is a valid index into it.
 * @post Returns @c ClockTicks(0) when @p master_tf is null.
 */
[[nodiscard]] inline ClockTicks viewOriginMasterAbsoluteAtIndex(
        std::shared_ptr<TimeFrame> const & master_tf,
        TimeFrameIndex const index) {
    if (!master_tf) {
        return ClockTicks(0);
    }
    return master_tf->getTimeAtIndex(index);
}

/**
 * @brief Visible window span in master clock ticks for view-local orthographic projection.
 *
 * View-local X coordinates are expressed as @c (absolute_time - origin). The projection
 * right bound is therefore the clock-tick delta between @p end and @p start, not the
 * index delta.
 *
 * @pre @p master_tf must not be null.
 * @post Returns at least @c ClockTicks(1).
 */
[[nodiscard]] inline ClockTicks viewSpanMasterAbsolute(
        std::shared_ptr<TimeFrame> const & master_tf,
        TimeFrameIndex const start,
        TimeFrameIndex const end) {
    if (!master_tf) {
        throw std::runtime_error("viewSpanMasterAbsolute: master time frame is null");
    }
    auto const t0 = master_tf->getTimeAtIndex(start);
    auto const t1 = master_tf->getTimeAtIndex(end);
    return ClockTicks(std::max<int64_t>(t1 - t0, 1));
}

}// namespace DataViewer

#endif// DATAVIEWER_VIEW_WINDOW_HPP

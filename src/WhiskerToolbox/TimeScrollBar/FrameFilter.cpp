/**
 * @file FrameFilter.cpp
 * @brief Implementation of DataKeyFrameFilter and the scan utility.
 */

#include "FrameFilter.hpp"

#include "DataManager/DataManager.hpp"
#include "DataObjects/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cassert>

// =============================================================================
// DataKeyFrameFilter
// =============================================================================

DataKeyFrameFilter::DataKeyFrameFilter(std::shared_ptr<DataManager> data_manager,
                                       std::string key,
                                       bool skip_tracked)
    : _data_manager{std::move(data_manager)},
      _key{std::move(key)},
      _time_frame{nullptr},
      _skip_tracked{skip_tracked} {
    assert(_data_manager && "DataKeyFrameFilter: data_manager must not be null");
}

void DataKeyFrameFilter::setTimeFrame(std::shared_ptr<TimeFrame> time_frame) {
    _time_frame = std::move(time_frame);
}

bool DataKeyFrameFilter::shouldSkip(TimeFrameIndex index) const {
    if (!_data_manager || !_time_frame) {
        return false;
    }

    auto series = _data_manager->getData<DigitalIntervalSeries>(_key);
    if (!series) {
        return false;
    }

    bool const has_interval = series->hasIntervalAtTime(index, *_time_frame);
    return _skip_tracked ? has_interval : !has_interval;
}

// =============================================================================
// Scan Utility
// =============================================================================

namespace frame_filter {

int scanToNextNonSkipped(FrameFilter const & filter,
                         int start,
                         int direction,
                         int min_val,
                         int max_val) noexcept {
    int const range = max_val - min_val + 1;
    int frame = start;
    for (int i = 0; i < range; ++i) {
        if (frame < min_val || frame > max_val) {
            break;
        }
        if (!filter.shouldSkip(TimeFrameIndex(frame))) {
            return frame;
        }
        frame += direction;
    }
    return -1;
}

}// namespace frame_filter

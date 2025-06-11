
#include "TimeFrame.hpp"

#include <cmath>
#include <cstdint>// For int64_t

TimeFrame::TimeFrame(std::vector<int> const & times) {
    _times = times;
    _total_frame_count = static_cast<int>(times.size());
}

int TimeFrame::getTimeAtIndex(TimeIndex index) const {
    if (index < TimeIndex(0) || index.getValue() >= _times.size()) {
        std::cout << "Index " << index.getValue() << " out of range" << " for time frame of size " << _times.size() << std::endl;
        return 0;
    }
    return _times[static_cast<size_t>(index.getValue())];
}

int TimeFrame::getIndexAtTime(float time) const {
    // Binary search to find the index closest to the given time
    auto it = std::lower_bound(_times.begin(), _times.end(), time);

    // If exact match found
    if (it != _times.end() && static_cast<float>(*it) == time) {
        return static_cast<int>(std::distance(_times.begin(), it));
    }

    // If time is beyond the last time point
    if (it == _times.end()) {
        return static_cast<int>(_times.size() - 1);
    }

    // If time is before the first time point
    if (it == _times.begin()) {
        return 0;
    }

    // Find the closest time point
    auto prev = it - 1;
    if (std::abs(static_cast<float>(*prev) - time) <= std::abs(static_cast<float>(*it) - time)) {
        return static_cast<int>(std::distance(_times.begin(), prev));
    } else {
        return static_cast<int>(std::distance(_times.begin(), it));
    }
}

int TimeFrame::checkFrameInbounds(int frame_id) const {

    if (frame_id < 0) {
        frame_id = 0;
    } else if (frame_id >= _total_frame_count) {
        frame_id = _total_frame_count;
    }
    return frame_id;
}

int64_t getTimeIndexForSeries(TimeIndex source_index,
                              TimeFrame const * source_time_frame,
                              TimeFrame const * destination_time_frame) {
    if (source_time_frame == destination_time_frame) {
        // Frames are the same. The time value can be used directly.
        return source_index.getValue();
    } else {
        auto destination_index = destination_time_frame->getIndexAtTime(source_index.getValue());
        return destination_index;
    }
}

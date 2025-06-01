#ifndef TIMEFRAME_HPP
#define TIMEFRAME_HPP

#include <cmath>
#include <concepts>
#include <iostream>
#include <vector>

class TimeFrame {
public:
    TimeFrame() = default;
    explicit TimeFrame(std::vector<int> const & times);

    [[nodiscard]] int getTotalFrameCount() const { return _total_frame_count; };

    template<std::integral T>
    [[nodiscard]] int getTimeAtIndex(T index) const {
        if (index < 0 || static_cast<size_t>(index) >= _times.size()) {
            std::cout << "Index " << index << " out of range" << " for time frame of size " << _times.size() << std::endl;
            return 0;
        }
        return _times[static_cast<size_t>(index)];
    }

    [[nodiscard]] int getIndexAtTime(float time) const {
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

    [[nodiscard]] int checkFrameInbounds(int frame_id) const {

        if (frame_id < 0) {
            frame_id = 0;
        } else if (frame_id >= _total_frame_count) {
            frame_id = _total_frame_count;
        }
        return frame_id;
    }

protected:
private:
    std::vector<int> _times;
    int _total_frame_count{0};
};

/**
 * @brief Converts a time index from one TimeFrame to another.
 *
 * If the time value's original frame (`source_time_frame`) is the same as the series'
 * data frame (`destination_time_frame`), the time value is returned as is.
 * Otherwise, the time value (which is a time in `source_time_frame`) is converted
 * to an index within the `destination_time_frame`.
 *
 * @param time_value_in_source_frame The time value, expressed in `source_value_frame` coordinates.
 * @param source_time_frame The TimeFrame in which `time_value_in_source_frame` is defined.
 * @param destination_time_frame The TimeFrame associated with the data series being queried. This
 *                          also serves as the target frame for index conversion if frames differ.
 * @return The original `time_value_in_source_frame` if frames are the same object instance,
 *         or the corresponding index in `destination_time_frame` if frames are different.
 */
float getTimeIndexForSeries(int64_t time_value_in_source_frame,
                                 TimeFrame const & source_time_frame,
                                 TimeFrame const & destination_time_frame);


#endif// TIMEFRAME_HPP

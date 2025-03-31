#ifndef TIMEFRAME_HPP
#define TIMEFRAME_HPP

#include <cmath>
#include <concepts>
#include <iostream>
#include <vector>

class TimeFrame {
public:
    TimeFrame() = default;
    explicit TimeFrame(std::vector<int> times);
    void updateTotalFrameCount(int frame_count) { _total_frame_count = frame_count; };
    [[nodiscard]] int getTotalFrameCount() const { return _total_frame_count; };

    void updateLastLoadedFrame(int frame) { _last_loaded_frame = frame; };
    [[nodiscard]] int getLastLoadedFrame() const { return _last_loaded_frame; };

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
    int _last_loaded_frame{0};
    int _total_frame_count{0};
};


#endif// TIMEFRAME_HPP

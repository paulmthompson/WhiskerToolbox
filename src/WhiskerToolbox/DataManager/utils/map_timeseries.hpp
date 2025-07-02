#ifndef MAP_TIMESERIES_HPP
#define MAP_TIMESERIES_HPP

#include "TimeFrame.hpp"

#include <map>
#include <vector>
#include <algorithm>
#include <ranges>

template<typename T>
[[nodiscard]] bool clear_at_time(TimeFrameIndex const time, T & data) {
    auto it = data.find(time);
    if (it != data.end()) {
        data.erase(it);
        return true;
    }
    return false;
}

template<typename T>
[[nodiscard]] bool clear_at_time(TimeFrameIndex const time, size_t const index, T & data) {
    auto it = data.find(time);
    if (it != data.end()) {
        if (index >= it->second.size()) {
            return false;
        }
        it->second.erase(it->second.begin() + index);
        return true;
    }
    return false;
}

#endif // MAP_TIMESERIES_HPP
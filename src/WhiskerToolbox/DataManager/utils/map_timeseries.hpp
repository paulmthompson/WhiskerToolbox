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

template<typename T, typename M> 
void add_at_time(TimeFrameIndex const time, T const & data, M & data_map) {
    data_map[time].push_back(data);
}

template<typename T, typename M>
[[nodiscard]] std::vector<T> const & get_at_time(TimeFrameIndex const time, M & data, std::vector<T> const & empty) {
    auto it = data.find(time);
    if (it != data.end()) {
        return it->second;
    }
    return empty;
}

template<typename T, typename M>
[[nodiscard]] std::vector<T> const & get_at_time(TimeFrameIndex const time, 
                                                M & data, 
                                                std::vector<T> const & empty, 
                                                TimeFrame const * source_timeframe, 
                                                TimeFrame const * target_timeframe) {

    // If the timeframes are the same object, no conversion is needed
    if (source_timeframe == target_timeframe) {
        return get_at_time(time, data, empty);
    }

    // If either timeframe is null, fall back to original behavior
    if (!source_timeframe || !target_timeframe) {
        return get_at_time(time, data, empty);
    }

    // Convert the time index from source timeframe to target timeframe
    // 1. Get the time value from the source timeframe
    auto time_value = source_timeframe->getTimeAtIndex(time);
    
    // 2. Convert that time value to an index in the target timeframe  
    auto target_index = target_timeframe->getIndexAtTime(static_cast<float>(time_value));

    return get_at_time(target_index, data, empty);
}


#endif // MAP_TIMESERIES_HPP
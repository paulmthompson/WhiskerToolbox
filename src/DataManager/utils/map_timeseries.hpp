#ifndef MAP_TIMESERIES_HPP
#define MAP_TIMESERIES_HPP

#include "TimeFrame/TimeFrame.hpp"

#include <map>
#include <vector>
#include <algorithm>
#include <ranges>
#include <unordered_set>

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
        it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }
    return false;
}

template<typename T, typename M> 
void add_at_time(TimeFrameIndex const time, T const & data, M & data_map) {
    data_map[time].push_back(data);
}

template<typename T, typename M>
[[nodiscard]] std::vector<T> const & get_at_time(TimeFrameIndex const time, M const & data, std::vector<T> const & empty) {
    auto it = data.find(time);
    if (it != data.end()) {
        return it->second;
    }
    return empty;
}

template<typename T, typename M>
[[nodiscard]] std::vector<T> const & get_at_time(TimeFrameIndex const time, 
                                                M const & data, 
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


// Convert a time index between timeframes; falls back to original when null/equal
inline TimeFrameIndex convert_time_index(TimeFrameIndex const time,
                                         TimeFrame const * source_timeframe,
                                         TimeFrame const * target_timeframe) {
    if (source_timeframe == target_timeframe) {
        return time;
    }
    if (!source_timeframe || !target_timeframe) {
        return time;
    }
    auto const time_value = source_timeframe->getTimeAtIndex(time);
    auto const target_index = target_timeframe->getIndexAtTime(static_cast<float>(time_value));
    return target_index;
}

// Fill an output vector by extracting a field from a vector of entries
template <typename Entry, typename Out, typename Extractor>
inline void fill_extracted_vector(std::vector<Entry> const & entries,
                                  std::vector<Out> & out,
                                  Extractor extractor) {
    out.clear();
    out.reserve(entries.size());
    for (auto const & entry : entries) {
        out.push_back(extractor(entry));
    }
}

// Template function for moving entries by EntityIds (unordered_set variant for O(1) lookups)
template <typename SourceDataMap, typename TargetType, typename DataExtractor>
inline std::size_t move_by_entity_ids(SourceDataMap & source_data,
                                      TargetType & target,
                                      std::unordered_set<EntityId> const & entity_ids_set,
                                      bool const notify,
                                      DataExtractor extract_data) {
    std::size_t total_moved = 0;
    std::vector<std::pair<TimeFrameIndex, size_t>> entries_to_remove;

    for (auto const & [time, entries] : source_data) {
        for (size_t i = 0; i < entries.size(); ++i) {
            auto const & entry = entries[i];
            if (entity_ids_set.contains(entry.entity_id)) {
                target.addEntryAtTime(time, extract_data(entry), entry.entity_id, false);
                entries_to_remove.emplace_back(time, i);
                total_moved++;
            }
        }
    }

    std::ranges::sort(entries_to_remove,
                      [](auto const & a, auto const & b) {
                          if (a.first != b.first) return a.first > b.first;
                          return a.second > b.second;
                      });

    for (auto const & [time, index] : entries_to_remove) {
        auto it = source_data.find(time);
        if (it != source_data.end() && index < it->second.size()) {
            it->second.erase(it->second.begin() + static_cast<long>(index));
            if (it->second.empty()) {
                source_data.erase(it);
            }
        }
    }

    if (notify && total_moved > 0) {
        target.notifyObservers();
        // Note: Source notification for the source container should be handled by the calling class
    }

    return total_moved;
}

// Copy entries by EntityIds (unordered_set variant for O(1) lookups)
template <typename SourceDataMap, typename TargetType, typename DataExtractor>
inline std::size_t copy_by_entity_ids(SourceDataMap const & source_data,
                                      TargetType & target,
                                      std::unordered_set<EntityId> const & entity_ids_set,
                                      bool const notify,
                                      DataExtractor extract_data) {
    std::size_t total_copied = 0;
    for (auto const & [time, entries] : source_data) {
        for (auto const & entry : entries) {
            if (entity_ids_set.contains(entry.entity_id)) {
                target.addAtTime(time, extract_data(entry), false);
                total_copied++;
            }
        }
    }
    if (notify && total_copied > 0) {
        target.notifyObservers();
    }
    return total_copied;
}


#endif // MAP_TIMESERIES_HPP
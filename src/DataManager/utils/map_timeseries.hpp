#ifndef MAP_TIMESERIES_HPP
#define MAP_TIMESERIES_HPP

#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <map>
#include <ranges>
#include <unordered_set>
#include <vector>

class LineData;

// Template function for moving entries by EntityIds (unordered_set variant for O(1) lookups)
template<typename SourceDataMap, typename TargetType, typename DataExtractor>
inline std::size_t move_by_entity_ids(SourceDataMap & source_data,
                                      TargetType & target,
                                      std::unordered_set<EntityId> const & entity_ids_set,
                                      bool const notify,
                                      DataExtractor extract_data) {
    std::size_t total_moved = 0;
    std::vector<std::pair<TimeFrameIndex, size_t>> entries_to_remove;

    for (auto const & [time, entries]: source_data) {
        for (size_t i = 0; i < entries.size(); ++i) {
            auto const & entry = entries[i];
            if (entity_ids_set.contains(entry.entity_id)) {
                if constexpr (std::is_same_v<TargetType, LineData>) {
                    target.addEntryAtTime(time, extract_data(entry), entry.entity_id, NotifyObservers::No);
                } else {
                    target.addEntryAtTime(time, extract_data(entry), entry.entity_id, false);
                }
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

    for (auto const & [time, index]: entries_to_remove) {
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
template<typename SourceDataMap, typename TargetType, typename DataExtractor>
inline std::size_t copy_by_entity_ids(SourceDataMap const & source_data,
                                      TargetType & target,
                                      std::unordered_set<EntityId> const & entity_ids_set,
                                      bool const notify,
                                      DataExtractor extract_data) {
    std::size_t total_copied = 0;
    for (auto const & [time, entries]: source_data) {
        for (auto const & entry: entries) {
            if (entity_ids_set.contains(entry.entity_id)) {
                if constexpr (std::is_same_v<TargetType, LineData>) {
                    target.addAtTime(time, extract_data(entry), NotifyObservers::No);
                } else {
                    target.addAtTime(time, extract_data(entry), false);
                }
                total_copied++;
            }
        }
    }
    if (notify && total_copied > 0) {
        target.notifyObservers();
    }
    return total_copied;
}


#endif// MAP_TIMESERIES_HPP
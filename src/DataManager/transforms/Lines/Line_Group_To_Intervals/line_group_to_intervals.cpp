#include "line_group_to_intervals.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/interval_data.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

namespace {  // NOLINT(cert-dcl59-cpp)
    /**
     * @brief Helper to merge nearby intervals
     * 
     * @param intervals Input intervals (must be sorted by start time)
     * @param gap_threshold Maximum gap size to merge across
     * @return Merged intervals
     */
    std::vector<Interval> mergeIntervals(std::vector<Interval> const & intervals, int gap_threshold) {
        if (intervals.empty()) {
            return {};
        }

        std::vector<Interval> merged;
        merged.reserve(intervals.size());
        merged.push_back(intervals[0]);

        for (size_t i = 1; i < intervals.size(); ++i) {
            auto & last = merged.back();
            auto const & current = intervals[i];

            // Check if current interval is within gap_threshold of the last one
            int64_t const gap = current.start - last.end;
            if (gap <= gap_threshold) {
                // Merge by extending the last interval
                last.end = current.end;
            } else {
                // No merge, add as new interval
                merged.push_back(current);
            }
        }

        return merged;
    }

    /**
     * @brief Helper to filter intervals by minimum length
     * 
     * @param intervals Input intervals
     * @param min_length Minimum interval length (inclusive)
     * @return Filtered intervals
     */
    std::vector<Interval> filterByLength(std::vector<Interval> const & intervals, int min_length) {
        if (min_length <= 1) {
            return intervals; // No filtering needed
        }

        std::vector<Interval> filtered;
        filtered.reserve(intervals.size());

        for (auto const & interval : intervals) {
            int64_t const length = interval.end - interval.start + 1; // Inclusive length
            if (length >= min_length) {
                filtered.push_back(interval);
            }
        }

        return filtered;
    }
}

std::shared_ptr<DigitalIntervalSeries> lineGroupToIntervals(
    std::shared_ptr<LineData> const & line_data,
    LineGroupToIntervalsParameters const * params) {
    // Delegate to version with progress callback
    return lineGroupToIntervals(line_data, params, [](int) {/* no progress reporting */});
}

std::shared_ptr<DigitalIntervalSeries> lineGroupToIntervals(
    std::shared_ptr<LineData> const & line_data,
    LineGroupToIntervalsParameters const * params,
    ProgressCallback progressCallback) {
    
    // Validate inputs
    if (!line_data || !params) {
        std::cerr << "lineGroupToIntervals: Invalid input (null line_data or params)" << std::endl;
        return nullptr;
    }

    if (!params->group_manager) {
        std::cerr << "lineGroupToIntervals: EntityGroupManager is null" << std::endl;
        return nullptr;
    }

    if (params->target_group_id == 0) {
        std::cerr << "lineGroupToIntervals: Invalid target_group_id (0)" << std::endl;
        return nullptr;
    }

    // Check if the group exists
    if (!params->group_manager->hasGroup(params->target_group_id)) {
        std::cerr << "lineGroupToIntervals: Target group " << params->target_group_id 
                  << " does not exist" << std::endl;
        return nullptr;
    }

    progressCallback(0);

    // Get all entities in the target group
    auto group_entities = params->group_manager->getEntitiesInGroup(params->target_group_id);
    if (group_entities.empty()) {
        std::cerr << "lineGroupToIntervals: Target group " << params->target_group_id 
                  << " is empty" << std::endl;
        return std::make_shared<DigitalIntervalSeries>(); // Return empty series
    }

    // Convert to unordered_set for O(1) lookup
    std::unordered_set<EntityId> const group_entity_set(group_entities.begin(), group_entities.end());

    // Get all times with data
    auto times_view = line_data->getTimesWithData();
    std::vector<TimeFrameIndex> all_times(times_view.begin(), times_view.end());
    
    if (all_times.empty()) {
        std::cerr << "lineGroupToIntervals: No data found in LineData" << std::endl;
        return std::make_shared<DigitalIntervalSeries>(); // Return empty series
    }

    // Sort times to ensure sequential processing
    std::ranges::sort(all_times);

    progressCallback(10);

    // For each time, check if any entity at that time is in the target group
    std::vector<bool> frame_active(all_times.size(), false);
    
    for (size_t i = 0; i < all_times.size(); ++i) {
        auto const time = all_times[i];
        auto entity_ids = line_data->getEntityIdsAtTime(time);
        
        // Check if any entity is in the group
        bool has_group_member = false;
        for (auto entity_id : entity_ids) {
            if (group_entity_set.contains(entity_id)) {
                has_group_member = true;
                break;
            }
        }

        // Set frame_active based on track_presence parameter
        frame_active[i] = params->track_presence ? has_group_member : !has_group_member;

        // Update progress periodically
        if (i % 100 == 0 || i == all_times.size() - 1) {
            int const progress = 10 + static_cast<int>((static_cast<double>(i) * 60.0) / static_cast<double>(all_times.size()));
            progressCallback(progress);
        }
    }

    progressCallback(70);

    // Build intervals from consecutive active frames
    std::vector<Interval> intervals;
    intervals.reserve(all_times.size() / 2); // Rough estimate

    int64_t interval_start = -1;
    for (size_t i = 0; i < all_times.size(); ++i) {
        if (frame_active[i]) {
            if (interval_start == -1) {
                // Start new interval
                interval_start = all_times[i].getValue();
            }
        } else {
            if (interval_start != -1) {
                // End current interval
                int64_t const interval_end = all_times[i - 1].getValue();
                intervals.push_back(Interval{interval_start, interval_end});
                interval_start = -1;
            }
        }
    }

    // Handle case where last frame is active
    if (interval_start != -1) {
        int64_t const interval_end = all_times.back().getValue();
        intervals.push_back(Interval{interval_start, interval_end});
    }

    progressCallback(80);

    // Apply merging if requested
    if (params->merge_gap_threshold > 1) {
        intervals = mergeIntervals(intervals, params->merge_gap_threshold);
    }

    progressCallback(90);

    // Apply minimum length filtering
    if (params->min_interval_length > 1) {
        intervals = filterByLength(intervals, params->min_interval_length);
    }

    progressCallback(95);

    // Create the DigitalIntervalSeries
    auto result = std::make_shared<DigitalIntervalSeries>(intervals);

    progressCallback(100);

    std::cout << "lineGroupToIntervals: Created " << intervals.size() 
              << " intervals for group " << params->target_group_id << std::endl;

    return result;
}

// ========== LineGroupToIntervalsOperation Implementation ==========

std::string LineGroupToIntervalsOperation::getName() const {
    return "Line Group to Digital Intervals";
}

std::type_index LineGroupToIntervalsOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineGroupToIntervalsOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineGroupToIntervalsOperation::getDefaultParameters() const {
    // Return default parameters - user must set group_manager and target_group_id
    return std::make_unique<LineGroupToIntervalsParameters>();
}

DataTypeVariant LineGroupToIntervalsOperation::execute(
    DataTypeVariant const & dataVariant,
    TransformParametersBase const * transformParameters) {
    // Delegate to version with progress callback
    return execute(dataVariant, transformParameters, [](int) {/* no-op */});
}

DataTypeVariant LineGroupToIntervalsOperation::execute(
    DataTypeVariant const & dataVariant,
    TransformParametersBase const * transformParameters,
    ProgressCallback progressCallback) {
    
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        std::cerr << "LineGroupToIntervalsOperation: Invalid input type" << std::endl;
        return dataVariant;
    }

    auto line_data = std::get<std::shared_ptr<LineData>>(dataVariant);

    auto const * params = dynamic_cast<LineGroupToIntervalsParameters const *>(transformParameters);
    if (!params) {
        std::cerr << "LineGroupToIntervalsOperation: Invalid parameters type" << std::endl;
        return dataVariant;
    }

    auto result = lineGroupToIntervals(line_data, params, progressCallback);
    
    if (!result) {
        std::cerr << "LineGroupToIntervalsOperation: Failed to create intervals" << std::endl;
        return dataVariant;
    }

    return DataTypeVariant{result};
}

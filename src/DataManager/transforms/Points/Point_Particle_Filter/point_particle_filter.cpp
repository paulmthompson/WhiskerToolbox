#include "point_particle_filter.hpp"

#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "StateEstimation/MaskParticleFilter.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <unordered_set>
#include <vector>

namespace {

/**
 * @brief Convert Point2D<float> to Point2D<uint32_t>
 */
Point2D<uint32_t> toUint32Point(Point2D<float> const & p) {
    return {
        static_cast<uint32_t>(std::max(0.0f, std::round(p.x))),
        static_cast<uint32_t>(std::max(0.0f, std::round(p.y)))
    };
}

/**
 * @brief Convert Point2D<uint32_t> to Point2D<float>
 */
Point2D<float> toFloatPoint(Point2D<uint32_t> const & p) {
    return {
        static_cast<float>(p.x),
        static_cast<float>(p.y)
    };
}

/**
 * @brief Extract ground truth labels for a specific group ID
 * 
 * @param point_data The point data containing labeled points
 * @param group_manager The group manager for accessing entity groups
 * @param group_id The group ID to extract
 * @return Map from time frame to point position (sorted by time)
 */
std::map<TimeFrameIndex, Point2D<float>> extractGroundTruthForGroup(
    PointData const * point_data,
    EntityGroupManager const * group_manager,
    GroupId group_id) {
    
    std::map<TimeFrameIndex, Point2D<float>> ground_truth;
    
    if (!group_manager) {
        return ground_truth;
    }
    
    // Get all entities in this group and convert to set for fast lookup
    auto entities_vec = group_manager->getEntitiesInGroup(group_id);
    std::unordered_set<EntityId> entities_in_group(entities_vec.begin(), entities_vec.end());
    
    // Iterate through all times with data using the range API
    for (auto const & timePointEntriesPair : point_data->GetAllPointEntriesAsRange()) {
        // Find points belonging to this group
        for (auto const & entry : timePointEntriesPair.entries) {
            // Check if this entity belongs to the target group
            if (entities_in_group.find(entry.entity_id) != entities_in_group.end()) {
                ground_truth[timePointEntriesPair.time] = entry.point;
                break;  // Assume one point per group per frame
            }
        }
    }
    
    return ground_truth;
}

/**
 * @brief Track a single segment between two ground truth labels
 * 
 * @param start_time Starting frame
 * @param end_time Ending frame
 * @param start_point Ground truth start point
 * @param end_point Ground truth end point
 * @param mask_data Mask data for all frames
 * @param tracker Particle filter tracker
 * @return Vector of tracked points (one per frame from start_time to end_time inclusive)
 */
std::vector<Point2D<float>> trackSegment(
    TimeFrameIndex start_time,
    TimeFrameIndex end_time,
    Point2D<float> const & start_point,
    Point2D<float> const & end_point,
    MaskData const * mask_data,
    StateEstimation::MaskPointTracker & tracker) {
    
    if (start_time.getValue() > end_time.getValue()) {
        return {};
    }
    
    // Collect masks for this time range
    std::vector<Mask2D> masks;
    size_t num_frames = static_cast<size_t>(end_time.getValue() - start_time.getValue() + 1);
    masks.reserve(num_frames);
    
    for (int64_t t_val = start_time.getValue(); t_val <= end_time.getValue(); ++t_val) {
        TimeFrameIndex t(t_val);
        auto mask_at_t = mask_data->getAtTime(t);
        
        if (mask_at_t.empty()) {
            // No mask at this time - use empty mask (tracker will handle it)
            masks.emplace_back();
        } else {
            // Flatten all masks at this time into one
            Mask2D combined_mask;
            for (auto const & m : mask_at_t) {
                combined_mask.insert(combined_mask.end(), m.begin(), m.end());
            }
            masks.push_back(std::move(combined_mask));
        }
    }
    
    // Convert float points to uint32_t for the particle filter
    Point2D<uint32_t> start_uint = toUint32Point(start_point);
    Point2D<uint32_t> end_uint = toUint32Point(end_point);
    
    // Run particle filter
    auto tracked_uint = tracker.track(start_uint, end_uint, masks);
    
    // Convert back to float
    std::vector<Point2D<float>> tracked_float;
    tracked_float.reserve(tracked_uint.size());
    for (auto const & p : tracked_uint) {
        tracked_float.push_back(toFloatPoint(p));
    }
    
    return tracked_float;
}

/**
 * @brief Track all segments for a single group
 * 
 * @param ground_truth Map of time -> point for this group
 * @param mask_data Mask data for constraint
 * @param tracker Particle filter tracker
 * @return Map of time -> tracked point for all frames between ground truth labels
 */
std::map<TimeFrameIndex, Point2D<float>> trackGroup(
    std::map<TimeFrameIndex, Point2D<float>> const & ground_truth,
    MaskData const * mask_data,
    StateEstimation::MaskPointTracker & tracker) {
    
    std::map<TimeFrameIndex, Point2D<float>> result;
    
    if (ground_truth.size() < 2) {
        // Not enough ground truth to track - just return what we have
        return ground_truth;
    }
    
    // Convert map to vector for easier iteration
    std::vector<std::pair<TimeFrameIndex, Point2D<float>>> gt_vec(
        ground_truth.begin(), ground_truth.end());
    
    // Track each segment between consecutive ground truth labels
    for (size_t i = 0; i < gt_vec.size() - 1; ++i) {
        TimeFrameIndex start_time = gt_vec[i].first;
        TimeFrameIndex end_time = gt_vec[i + 1].first;
        Point2D<float> const & start_point = gt_vec[i].second;
        Point2D<float> const & end_point = gt_vec[i + 1].second;
        
        auto tracked_segment = trackSegment(
            start_time, end_time, start_point, end_point, mask_data, tracker);
        
        // Add tracked points to result
        for (size_t j = 0; j < tracked_segment.size(); ++j) {
            TimeFrameIndex time(start_time.getValue() + static_cast<int64_t>(j));
            result[time] = tracked_segment[j];
        }
    }
    
    return result;
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<PointData> pointParticleFilter(
    PointData const * point_data,
    MaskData const * mask_data,
    EntityGroupManager * group_manager,
    size_t num_particles,
    float transition_radius,
    float random_walk_prob) {
    
    return pointParticleFilter(
        point_data, mask_data, group_manager, num_particles, transition_radius, random_walk_prob,
        [](int) { /* no-op progress */ });
}

std::shared_ptr<PointData> pointParticleFilter(
    PointData const * point_data,
    MaskData const * mask_data,
    EntityGroupManager * group_manager,
    size_t num_particles,
    float transition_radius,
    float random_walk_prob,
    ProgressCallback progressCallback) {
    
    if (!point_data || !mask_data) {
        std::cerr << "PointParticleFilter: Null PointData or MaskData provided." << std::endl;
        if (progressCallback) progressCallback(100);
        return std::make_shared<PointData>();
    }
    
    if (!group_manager) {
        std::cerr << "PointParticleFilter: No EntityGroupManager provided." << std::endl;
        if (progressCallback) progressCallback(100);
        return std::make_shared<PointData>();
    }
    
    if (progressCallback) progressCallback(0);
    
    // Create result PointData (copy structure from input)
    auto result = std::make_shared<PointData>();
    result->setImageSize(point_data->getImageSize());
    
    // Get all unique group IDs
    auto all_group_ids = group_manager->getAllGroupIds();
    std::set<GroupId> group_ids(all_group_ids.begin(), all_group_ids.end());
    
    if (group_ids.empty()) {
        std::cerr << "PointParticleFilter: No grouped points found in PointData." << std::endl;
        if (progressCallback) progressCallback(100);
        return result;
    }
    
    // Create particle filter tracker
    StateEstimation::MaskPointTracker tracker(
        num_particles, transition_radius, random_walk_prob);
    
    // Track each group independently
    size_t group_count = 0;
    for (GroupId group_id : group_ids) {
        // Extract ground truth for this group
        auto ground_truth = extractGroundTruthForGroup(point_data, group_manager, group_id);
        
        // Get all entities in this group and convert to set for fast lookup
        auto entities_vec = group_manager->getEntitiesInGroup(group_id);
        std::unordered_set<EntityId> entities_in_group(entities_vec.begin(), entities_vec.end());
        
        if (ground_truth.size() < 2) {
            // Not enough ground truth - just copy what we have
            for (auto const & [time, point] : ground_truth) {
                for (auto const & timePointEntriesPair : point_data->GetAllPointEntriesAsRange()) {
                    if (timePointEntriesPair.time == time) {
                        for (auto const & entry : timePointEntriesPair.entries) {
                            if (entities_in_group.find(entry.entity_id) != entities_in_group.end()) {
                                result->addPointEntryAtTime(time, entry.point, entry.entity_id, false);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        } else {
            // Track this group through masks
            auto tracked = trackGroup(ground_truth, mask_data, tracker);
            
            // Find an entity ID from this group to use for all tracked points
            EntityId representative_entity_id = 0;
            if (!entities_vec.empty()) {
                representative_entity_id = entities_vec.front();
            }
            
            // Add tracked points to result
            for (auto const & [time, point] : tracked) {
                result->addPointEntryAtTime(time, point, representative_entity_id, false);
            }
        }
        
        // Update progress
        ++group_count;
        if (progressCallback) {
            auto progress = static_cast<int>(100.0 * static_cast<double>(group_count) / static_cast<double>(group_ids.size()));
            progressCallback(progress);
        }
    }
    
    if (progressCallback) progressCallback(100);
    
    return result;
}

///////////////////////////////////////////////////////////////////////////////

std::string PointParticleFilterOperation::getName() const {
    return "Track Points Through Masks (Particle Filter)";
}

std::type_index PointParticleFilterOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<PointData>);
}

bool PointParticleFilterOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<PointData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<PointData>>(&dataVariant);

    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> PointParticleFilterOperation::getDefaultParameters() const {
    return std::make_unique<PointParticleFilterParameters>();
}

DataTypeVariant PointParticleFilterOperation::execute(
    DataTypeVariant const & dataVariant,
    TransformParametersBase const * transformParameters) {
    
    return execute(dataVariant, transformParameters, [](int) { /* no-op progress */ });
}

DataTypeVariant PointParticleFilterOperation::execute(
    DataTypeVariant const & dataVariant,
    TransformParametersBase const * transformParameters,
    ProgressCallback progressCallback) {
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<PointData>>(&dataVariant);
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "PointParticleFilterOperation::execute: Incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto point_data = *ptr_ptr;

    auto const * typed_params =
        transformParameters ? dynamic_cast<PointParticleFilterParameters const *>(transformParameters) : nullptr;

    if (!typed_params || !typed_params->mask_data || !typed_params->group_manager) {
        std::cerr << "PointParticleFilterOperation::execute: Missing mask data or group manager in parameters." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    if (progressCallback) progressCallback(0);
    
    std::shared_ptr<PointData> result = ::pointParticleFilter(
        point_data.get(),
        typed_params->mask_data.get(),
        typed_params->group_manager,
        typed_params->num_particles,
        typed_params->transition_radius,
        typed_params->random_walk_prob,
        progressCallback
    );

    if (!result) {
        std::cerr << "PointParticleFilterOperation::execute: Filtering failed." << std::endl;
        return {};
    }

    std::cout << "PointParticleFilterOperation executed successfully." << std::endl;
    return result;
}

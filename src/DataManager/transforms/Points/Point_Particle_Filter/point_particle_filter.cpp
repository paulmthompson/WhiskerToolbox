#include "point_particle_filter.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "StateEstimation/MaskParticleFilter.hpp"
#include "transforms/utils/variant_type_check.hpp"

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
 * @param scale_x Scaling factor for x coordinates (point space -> mask space)
 * @param scale_y Scaling factor for y coordinates (point space -> mask space)
 * @return Map from time frame to point position (in mask coordinate space)
 */
std::map<TimeFrameIndex, Point2D<float>> extractGroundTruthForGroup(
    PointData const * point_data,
    EntityGroupManager const * group_manager,
    GroupId group_id,
    float scale_x,
    float scale_y) {
    
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
                // Apply scaling to convert from point space to mask space
                Point2D<float> scaled_point{
                    entry.data.x * scale_x,
                    entry.data.y * scale_y
                };
                ground_truth[timePointEntriesPair.time] = scaled_point;
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
 * @param start_point Ground truth start point (in mask coordinate space)
 * @param end_point Ground truth end point (in mask coordinate space)
 * @param mask_data Mask data for all frames
 * @param tracker Particle filter tracker
 * @param frames_completed Accumulated frame count for progress tracking (modified in-place)
 * @param total_frames Total frames to track across all segments
 * @param progressCallback Optional callback for progress updates
 * @param inv_scale_x Inverse scaling factor for x coordinates (mask space -> point space)
 * @param inv_scale_y Inverse scaling factor for y coordinates (mask space -> point space)
 * @return Vector of tracked points (one per frame, in original point coordinate space)
 */
std::vector<Point2D<float>> trackSegment(
    TimeFrameIndex start_time,
    TimeFrameIndex end_time,
    Point2D<float> const & start_point,
    Point2D<float> const & end_point,
    MaskData const * mask_data,
    StateEstimation::MaskPointTracker & tracker,
    size_t & frames_completed,
    size_t total_frames,
    ProgressCallback progressCallback,
    float inv_scale_x,
    float inv_scale_y) {
    
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
    
    // Convert back to float and apply inverse scaling to return to original point coordinate space
    std::vector<Point2D<float>> tracked_float;
    tracked_float.reserve(tracked_uint.size());
    for (auto const & p : tracked_uint) {
        Point2D<float> point_in_mask_space = toFloatPoint(p);
        Point2D<float> point_in_point_space{
            point_in_mask_space.x * inv_scale_x,
            point_in_mask_space.y * inv_scale_y
        };
        tracked_float.push_back(point_in_point_space);
    }
    
    // Update progress
    frames_completed += tracked_uint.size();
    if (progressCallback && total_frames > 0) {
        auto progress = static_cast<int>(100.0 * static_cast<double>(frames_completed) / static_cast<double>(total_frames));
        progressCallback(std::min(progress, 99));  // Reserve 100 for completion
    }
    
    return tracked_float;
}

/**
 * @brief Track all segments for a single group
 * 
 * @param ground_truth Map of time -> point for this group (in mask coordinate space)
 * @param mask_data Mask data for constraint
 * @param tracker Particle filter tracker
 * @param frames_completed Accumulated frame count for progress tracking (modified in-place)
 * @param total_frames Total frames to track across all segments
 * @param progressCallback Optional callback for progress updates
 * @param inv_scale_x Inverse scaling factor for x coordinates (mask space -> point space)
 * @param inv_scale_y Inverse scaling factor for y coordinates (mask space -> point space)
 * @return Map of time -> tracked point for all frames (in original point coordinate space)
 */
std::map<TimeFrameIndex, Point2D<float>> trackGroup(
    std::map<TimeFrameIndex, Point2D<float>> const & ground_truth,
    MaskData const * mask_data,
    StateEstimation::MaskPointTracker & tracker,
    size_t & frames_completed,
    size_t total_frames,
    ProgressCallback progressCallback,
    float inv_scale_x,
    float inv_scale_y) {
    
    std::map<TimeFrameIndex, Point2D<float>> result;
    
    if (ground_truth.size() < 2) {
        // Not enough ground truth to track - just return what we have
        return ground_truth;
    }
    
    // Convert map to vector for easier iteration
    std::vector<std::pair<TimeFrameIndex, Point2D<float>>> gt_vec(
        ground_truth.begin(), ground_truth.end());
    
    // First, add all ground truth labels to preserve them exactly
    // Ground truth is in mask space, so convert back to point space
    for (auto const & [time, point] : ground_truth) {
        Point2D<float> point_in_point_space{
            point.x * inv_scale_x,
            point.y * inv_scale_y
        };
        result[time] = point_in_point_space;
    }
    
    // Track each segment between consecutive ground truth labels
    for (size_t i = 0; i < gt_vec.size() - 1; ++i) {
        TimeFrameIndex start_time = gt_vec[i].first;
        TimeFrameIndex end_time = gt_vec[i + 1].first;
        Point2D<float> const & start_point = gt_vec[i].second;
        Point2D<float> const & end_point = gt_vec[i + 1].second;
        
        auto tracked_segment = trackSegment(
            start_time, end_time, start_point, end_point, mask_data, tracker,
            frames_completed, total_frames, progressCallback, inv_scale_x, inv_scale_y);
        
        // Add tracked points ONLY for intermediate frames (skip start and end)
        // tracked_segment[0] corresponds to start_time (skip - has ground truth)
        // tracked_segment[last] corresponds to end_time (skip - has ground truth)
        for (size_t j = 1; j < tracked_segment.size() - 1; ++j) {
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
    float random_walk_prob,
    bool use_velocity_model,
    float velocity_noise_std) {
    
    return pointParticleFilter(
        point_data, mask_data, group_manager, num_particles, transition_radius, random_walk_prob,
        use_velocity_model, velocity_noise_std,
        [](int) { /* no-op progress */ });
}

std::shared_ptr<PointData> pointParticleFilter(
    PointData const * point_data,
    MaskData const * mask_data,
    EntityGroupManager * group_manager,
    size_t num_particles,
    float transition_radius,
    float random_walk_prob,
    bool use_velocity_model,
    float velocity_noise_std,
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
    
    // Check image sizes and scale if necessary
    ImageSize point_size = point_data->getImageSize();
    ImageSize mask_size = mask_data->getImageSize();
    
    bool needs_scaling = (point_size != mask_size);
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    
    if (needs_scaling) {
        if (point_size.width <= 0 || point_size.height <= 0) {
            std::cerr << "PointParticleFilter: Invalid PointData image size ("
                      << point_size.width << " x " << point_size.height << ")." << std::endl;
            if (progressCallback) progressCallback(100);
            return std::make_shared<PointData>();
        }
        
        if (mask_size.width <= 0 || mask_size.height <= 0) {
            std::cerr << "PointParticleFilter: Invalid MaskData image size ("
                      << mask_size.width << " x " << mask_size.height << ")." << std::endl;
            if (progressCallback) progressCallback(100);
            return std::make_shared<PointData>();
        }
        
        // Calculate scaling factors: point_space -> mask_space
        scale_x = static_cast<float>(mask_size.width) / static_cast<float>(point_size.width);
        scale_y = static_cast<float>(mask_size.height) / static_cast<float>(point_size.height);
        
        std::cout << "PointParticleFilter: Image size mismatch detected." << std::endl;
        std::cout << "  Point data: " << point_size.width << " x " << point_size.height << std::endl;
        std::cout << "  Mask data:  " << mask_size.width << " x " << mask_size.height << std::endl;
        std::cout << "  Applying scaling: " << scale_x << " x " << scale_y << std::endl;
    }
    
    // Create result PointData (with original point data image size)
    auto result = std::make_shared<PointData>();
    result->setImageSize(point_size);
    
    // Get all unique group IDs
    auto all_group_ids = group_manager->getAllGroupIds();
    std::set<GroupId> group_ids(all_group_ids.begin(), all_group_ids.end());
    
    if (group_ids.empty()) {
        std::cerr << "PointParticleFilter: No grouped points found in PointData." << std::endl;
        if (progressCallback) progressCallback(100);
        return result;
    }
    
    // Calculate inverse scaling factors for converting results back to point space
    float inv_scale_x = 1.0f / scale_x;
    float inv_scale_y = 1.0f / scale_y;
    
    // First pass: Calculate total number of frames to track for progress reporting
    size_t total_frames = 0;
    for (GroupId group_id : group_ids) {
        auto ground_truth = extractGroundTruthForGroup(point_data, group_manager, group_id, scale_x, scale_y);
        if (ground_truth.size() >= 2) {
            // Convert to vector for easier iteration
            std::vector<std::pair<TimeFrameIndex, Point2D<float>>> gt_vec(
                ground_truth.begin(), ground_truth.end());
            
            // Sum up frames in all segments
            for (size_t i = 0; i < gt_vec.size() - 1; ++i) {
                int64_t segment_frames = gt_vec[i + 1].first.getValue() - gt_vec[i].first.getValue() + 1;
                total_frames += static_cast<size_t>(segment_frames);
            }
        }
    }
    
    // Create particle filter tracker with velocity model if enabled
    StateEstimation::MaskPointTracker tracker(
        num_particles, transition_radius, random_walk_prob, 
        use_velocity_model, velocity_noise_std);
    
    // Track each group independently
    size_t frames_completed = 0;
    for (GroupId group_id : group_ids) {
        // Extract ground truth for this group (scaled to mask coordinate space)
        auto ground_truth = extractGroundTruthForGroup(point_data, group_manager, group_id, scale_x, scale_y);
        
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
                                result->addEntryAtTime(time, entry.data, entry.entity_id, false);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        } else {
            // Track this group through masks (results will be scaled back to point space)
            auto tracked = trackGroup(ground_truth, mask_data, tracker,
                                     frames_completed, total_frames, progressCallback,
                                     inv_scale_x, inv_scale_y);
            
            // Find an entity ID from this group to use for all tracked points
            EntityId representative_entity_id = EntityId(0);
            if (!entities_vec.empty()) {
                representative_entity_id = entities_vec.front();
            }
            
            // Add tracked points to result (already in original point coordinate space)
            for (auto const & [time, point] : tracked) {
                result->addEntryAtTime(time, point, representative_entity_id, false);
            }
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
    return canApplyToType<PointData>(dataVariant);
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
        typed_params->use_velocity_model,
        typed_params->velocity_noise_std,
        progressCallback
    );

    if (!result) {
        std::cerr << "PointParticleFilterOperation::execute: Filtering failed." << std::endl;
        return {};
    }

    std::cout << "PointParticleFilterOperation executed successfully." << std::endl;
    return result;
}

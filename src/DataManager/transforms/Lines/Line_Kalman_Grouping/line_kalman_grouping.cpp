#include "line_kalman_grouping.hpp"

#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "CoreGeometry/line_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <ranges>

Eigen::Vector2d calculateLineCentroid(Line2D const& line) {
    if (line.empty()) {
        return Eigen::Vector2d::Zero();
    }
    
    double sum_x = 0.0;
    double sum_y = 0.0;
    
    for (auto const& point : line) {
        sum_x += static_cast<double>(point.x);
        sum_y += static_cast<double>(point.y);
    }
    
    double count = static_cast<double>(line.size());
    return Eigen::Vector2d(sum_x / count, sum_y / count);
}

// CachedCentroidExtractor implementation
Eigen::VectorXd CachedCentroidExtractor::getFilterFeatures(const TrackedEntity& data) const {
    auto it = centroid_cache_.find(data.id);
    if (it == centroid_cache_.end()) {
        return Eigen::VectorXd::Zero(2);
    }
    
    const Eigen::Vector2d& centroid = it->second;
    Eigen::VectorXd features(2);
    features << centroid.x(), centroid.y();
    return features;
}

StateEstimation::FeatureCache CachedCentroidExtractor::getAllFeatures(const TrackedEntity& data) const {
    StateEstimation::FeatureCache cache;
    cache[getFilterFeatureName()] = getFilterFeatures(data);
    return cache;
}

std::string CachedCentroidExtractor::getFilterFeatureName() const {
    return "kalman_features";
}

StateEstimation::FilterState CachedCentroidExtractor::getInitialState(const TrackedEntity& data) const {
    Eigen::VectorXd initialState(4);
    
    auto it = centroid_cache_.find(data.id);
    if (it == centroid_cache_.end()) {
        initialState.setZero();
    } else {
        const Eigen::Vector2d& centroid = it->second;
        initialState << centroid.x(), centroid.y(), 0, 0; // Position from centroid, velocity is zero
    }

    Eigen::MatrixXd p(4, 4);
    p.setIdentity();
    p *= 100.0; // High initial uncertainty

    return {.state_mean = initialState, .state_covariance = p};
}

std::unique_ptr<StateEstimation::IFeatureExtractor<TrackedEntity>> CachedCentroidExtractor::clone() const {
    return std::make_unique<CachedCentroidExtractor>(*this);
}



std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params) {
    // No-op progress callback
    return ::lineKalmanGrouping(std::move(line_data), params, [](int) {/* no progress reporting */});
}

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params,
                                             ProgressCallback const& progressCallback) {
    if (!line_data || !params || !params->getGroupManager()) {
        return line_data;
    }
    
    using namespace StateEstimation;
    
    auto group_manager = params->getGroupManager();
    
    // Get all time frames with data
    auto times_view = line_data->getTimesWithData();
    std::vector<TimeFrameIndex> all_times(times_view.begin(), times_view.end());
    if (all_times.empty()) {
        progressCallback(100);
        return line_data;
    }
    
    std::sort(all_times.begin(), all_times.end());
    FrameIndex start_frame = static_cast<FrameIndex>(all_times.front().getValue());
    FrameIndex end_frame = static_cast<FrameIndex>(all_times.back().getValue());
    
    if (params->verbose_output) {
        std::cout << "Processing " << all_times.size() << " frames from " 
                  << start_frame << " to " << end_frame << std::endl;
    }
    
    // PHASE 1: Pre-compute all centroids and build cache
    // This eliminates the need to copy Line2D objects or hold pointers
    std::unordered_map<StateEstimation::EntityID, Eigen::Vector2d> centroid_cache;
    
    if (params->verbose_output) {
        std::cout << "Pre-computing centroids for all lines..." << std::endl;
    }
    
    for (auto time : all_times) {
        const auto& lines_at_time = line_data->getAtTime(time);
        const auto& entity_ids = line_data->getEntityIdsAtTime(time);
        
        for (size_t i = 0; i < lines_at_time.size() && i < entity_ids.size(); ++i) {
            centroid_cache[entity_ids[i]] = calculateLineCentroid(lines_at_time[i]);
        }
    }
    
    if (params->verbose_output) {
        std::cout << "Cached " << centroid_cache.size() << " centroids" << std::endl;
    }
    
    // Build DataSource: map from FrameIndex to vector of TrackedEntity (lightweight, no copying)
    Tracker<TrackedEntity>::DataSource data_source;
    
    for (auto time : all_times) {
        const auto& entity_ids = line_data->getEntityIdsAtTime(time);
        
        std::vector<TrackedEntity> frame_data;
        frame_data.reserve(entity_ids.size());
        
        for (auto entity_id : entity_ids) {
            frame_data.push_back({entity_id});
        }
        
        data_source[static_cast<FrameIndex>(time.getValue())] = std::move(frame_data);
    }
    
    // Build initial GroupMap from EntityGroupManager
    Tracker<TrackedEntity>::GroupMap groups;
    auto all_group_ids = group_manager->getAllGroupIds();
    for (auto group_id : all_group_ids) {
        auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
        groups[group_id] = std::vector<EntityID>(entities_in_group.begin(), entities_in_group.end());
    }
    
    // Build GroundTruthMap: frames where entities are already grouped
    // More efficient: iterate through groups instead of all entities at all frames
    Tracker<TrackedEntity>::GroundTruthMap ground_truth;
    for (auto group_id : all_group_ids) {
        const auto& entities_in_group = group_manager->getEntitiesInGroup(group_id);
        
        for (auto entity_id : entities_in_group) {
            // Find which frame this entity belongs to
            auto time_info = line_data->getTimeAndIndexByEntityId(entity_id);
            if (time_info.has_value()) {
                FrameIndex frame_idx = static_cast<FrameIndex>(time_info->first.getValue());
                ground_truth[frame_idx][group_id] = entity_id;
            }
        }
    }
    
    if (params->verbose_output) {
        std::cout << "Found " << all_group_ids.size() << " existing groups with " 
                  << ground_truth.size() << " ground truth frames" << std::endl;
    }
    
    // Create Kalman filter prototype
    double dt = params->dt;
    Eigen::MatrixXd F(4, 4);
    F << 1, 0, dt, 0,
         0, 1, 0, dt,
         0, 0, 1, 0,
         0, 0, 0, 1;
    
    Eigen::MatrixXd H(2, 4);
    H << 1, 0, 0, 0,
         0, 1, 0, 0;
    
    Eigen::MatrixXd Q(4, 4);
    Q << params->process_noise_position * params->process_noise_position, 0, 0, 0,
         0, params->process_noise_position * params->process_noise_position, 0, 0,
         0, 0, params->process_noise_velocity * params->process_noise_velocity, 0,
         0, 0, 0, params->process_noise_velocity * params->process_noise_velocity;
    
    Eigen::MatrixXd R(2, 2);
    R << params->measurement_noise * params->measurement_noise, 0,
         0, params->measurement_noise * params->measurement_noise;
    
    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<CachedCentroidExtractor>(std::move(centroid_cache));
    auto assigner = std::make_unique<HungarianAssigner>(
        params->max_assignment_distance, 
        H, 
        R, 
        "kalman_features"
    );
    
    // Create the tracker with lightweight TrackedEntity (no Line2D copying!)
    Tracker<TrackedEntity> tracker(
        std::move(kalman_filter), 
        std::move(feature_extractor), 
        std::move(assigner)
    );
    
    // Run the tracking process with progress callback
    // Wrap the DataManager's ProgressCallback into StateEstimation's ProgressCallback (both are compatible)
    StateEstimation::ProgressCallback se_callback = progressCallback;
    SmoothedResults smoothed_results = tracker.process(
        data_source, 
        groups, 
        ground_truth, 
        start_frame, 
        end_frame,
        se_callback  // Pass through the progress callback!
    );
    
    // Apply the group assignments back to the EntityGroupManager
    for (const auto& [group_id, entity_ids] : groups) {
        // Get existing entities
        auto existing_entities = group_manager->getEntitiesInGroup(group_id);
        std::unordered_set<EntityID> existing_set(existing_entities.begin(), existing_entities.end());
        
        // Add new entities that were assigned by the tracker
        for (auto entity_id : entity_ids) {
            if (!existing_set.contains(entity_id)) {
                group_manager->addEntityToGroup(group_id, entity_id);
                
                if (params->verbose_output) {
                    std::cout << "Added entity " << entity_id << " to group " << group_id << std::endl;
                }
            }
        }
    }
    
    progressCallback(100);
    return line_data;
}

// LineKalmanGroupingOperation implementation

std::string LineKalmanGroupingOperation::getName() const {
    return "Group Lines using Kalman Filtering";
}

std::type_index LineKalmanGroupingOperation::getTargetInputTypeIndex() const {
    return std::type_index(typeid(std::shared_ptr<LineData>));
}

bool LineKalmanGroupingOperation::canApply(DataTypeVariant const& dataVariant) const {
    return std::holds_alternative<std::shared_ptr<LineData>>(dataVariant) &&
           std::get<std::shared_ptr<LineData>>(dataVariant) != nullptr;
}

std::unique_ptr<TransformParametersBase> LineKalmanGroupingOperation::getDefaultParameters() const {
    // Note: Returns nullptr since we can't create a GroupingTransformParametersBase
    // without an EntityGroupManager pointer. The calling code will need to provide
    // the actual parameters with the group manager.
    return nullptr;
}

DataTypeVariant LineKalmanGroupingOperation::execute(DataTypeVariant const& dataVariant,
                                                     TransformParametersBase const* transformParameters) {
    // No-op progress callback
    return execute(dataVariant, transformParameters, [](int) {/* no progress reporting */});
}

DataTypeVariant LineKalmanGroupingOperation::execute(DataTypeVariant const& dataVariant,
                                                     TransformParametersBase const* transformParameters,
                                                     ProgressCallback progressCallback) {
    if (!canApply(dataVariant)) {
        return DataTypeVariant{};
    }
    
    auto line_data = std::get<std::shared_ptr<LineData>>(dataVariant);
    auto params = dynamic_cast<LineKalmanGroupingParameters const*>(transformParameters);
    
    if (!params) {
        return DataTypeVariant{};
    }
    
    auto result = ::lineKalmanGrouping(line_data, params, progressCallback);
    return DataTypeVariant{result};
}
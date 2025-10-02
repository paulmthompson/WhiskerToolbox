#include "line_kalman_grouping.hpp"

#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "StateEstimation/DataAdapter.hpp"
#include "StateEstimation/Features/LineCentroidExtractor.hpp"
#include "StateEstimation/Features/LineBasePointExtractor.hpp"
#include "StateEstimation/Features/CompositeFeatureExtractor.hpp"
#include "StateEstimation/Kalman/KalmanMatrixBuilder.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>



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
    TimeFrameIndex start_frame = all_times.front();
    TimeFrameIndex end_frame = all_times.back();
    
    if (params->verbose_output) {
        std::cout << "Processing " << all_times.size() << " frames from " 
                  << start_frame.getValue() << " to " << end_frame.getValue() << std::endl;
    }
    
    // Get natural iterator from LineData and flatten to individual items
    // This provides zero-copy access to Line2D objects
    auto line_entries_range = line_data->GetAllLineEntriesAsRange();
    auto data_source = StateEstimation::flattenLineData(line_entries_range);
    
    if (params->verbose_output) {
        std::cout << "Created zero-copy data source from LineData" << std::endl;
    }
    
    // Build GroundTruthMap: frames where entities are already grouped
    StateEstimation::Tracker<Line2D>::GroundTruthMap ground_truth;
    auto all_group_ids = group_manager->getAllGroupIds();
    
    for (auto group_id : all_group_ids) {
        const auto& entities_in_group = group_manager->getEntitiesInGroup(group_id);
        
        for (auto entity_id : entities_in_group) {
            // Find which frame this entity belongs to
            auto time_info = line_data->getTimeAndIndexByEntityId(entity_id);
            if (time_info.has_value()) {
                ground_truth[time_info->first][group_id] = entity_id;
            }
        }
    }
    
    if (params->verbose_output) {
        std::cout << "Found " << all_group_ids.size() << " existing groups with " 
                  << ground_truth.size() << " ground truth frames" << std::endl;
    }
    
    // Create composite feature extractor with centroid + base point
    // Uses metadata-driven approach to handle different feature types
    auto composite_extractor = std::make_unique<StateEstimation::CompositeFeatureExtractor<Line2D>>();
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineCentroidExtractor>());
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineBasePointExtractor>());
    
    // Get metadata from all child extractors
    // This automatically handles different temporal behaviors (kinematic, static, etc.)
    auto metadata_list = composite_extractor->getChildMetadata();
    
    if (params->verbose_output) {
        std::cout << "Building Kalman filter for " << metadata_list.size() << " features:" << std::endl;
        int total_meas = 0, total_state = 0;
        for (auto const& meta : metadata_list) {
            std::cout << "  - " << meta.name << ": " 
                      << meta.measurement_size << "D measurement → "
                      << meta.state_size << "D state";
            if (meta.hasDerivatives()) {
                std::cout << " (with derivatives)";
            }
            std::cout << std::endl;
            total_meas += meta.measurement_size;
            total_state += meta.state_size;
        }
        std::cout << "Total measurement space: " << total_meas << "D" << std::endl;
        std::cout << "Total state space: " << total_state << "D" << std::endl;
    }
    
    // Build Kalman matrices from metadata
    // This automatically creates correct block-diagonal structure
    StateEstimation::KalmanMatrixBuilder::FeatureConfig config;
    config.dt = params->dt;
    config.process_noise_position = params->process_noise_position;
    config.process_noise_velocity = params->process_noise_velocity;
    config.measurement_noise = params->measurement_noise;
    
    auto [F, H, Q, R] = StateEstimation::KalmanMatrixBuilder::buildAllMatricesFromMetadata(
        metadata_list, config);
    
    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    
    auto assigner = std::make_unique<HungarianAssigner>(
        params->max_assignment_distance, 
        H, 
        R, 
        "composite_features"  // Feature name from composite extractor
    );
    
    // Create the tracker with Line2D as the template parameter (correct type!)
    StateEstimation::Tracker<Line2D> tracker(
        std::move(kalman_filter), 
        std::move(composite_extractor), 
        std::move(assigner)
    );
    
    // Run the tracking process with zero-copy data source
    // Features are computed on-demand from Line2D references
    StateEstimation::ProgressCallback se_callback = progressCallback;
    [[maybe_unused]] auto smoothed_results = tracker.process(
        std::move(data_source),  // Move the adapter (it stores the range by value)
        *group_manager,          // Modified in-place
        ground_truth, 
        start_frame, 
        end_frame,
        se_callback
    );
    
    if (params->verbose_output) {
        std::cout << "Tracking complete. Groups updated in EntityGroupManager." << std::endl;
        for (auto group_id : all_group_ids) {
            auto entities = group_manager->getEntitiesInGroup(group_id);
            std::cout << "Group " << group_id << " now has " << entities.size() << " entities" << std::endl;
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
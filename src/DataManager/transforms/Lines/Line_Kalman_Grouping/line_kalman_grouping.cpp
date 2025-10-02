#include "line_kalman_grouping.hpp"

#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "StateEstimation/DataAdapter.hpp"
#include "StateEstimation/Features/LineCentroidExtractor.hpp"
#include "StateEstimation/Features/LineBasePointExtractor.hpp"
#include "StateEstimation/Features/LineLengthExtractor.hpp"
#include "StateEstimation/Features/CompositeFeatureExtractor.hpp"
#include "StateEstimation/Kalman/KalmanMatrixBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <ranges>

namespace {

/**
 * @brief Statistics for a feature extracted from ground truth data
 */
struct FeatureStatistics {
    double mean = 0.0;
    double variance = 0.0;
    double std_dev = 0.0;
    double mean_frame_to_frame_change = 0.0;
    double variance_frame_to_frame_change = 0.0;
    int num_samples = 0;
    int num_transitions = 0;
};

/**
 * @brief Analyze ground truth data to estimate realistic noise parameters
 * 
 * For static features (like length), computes:
 * - Mean and variance of the feature across all ground truth
 * - Mean and variance of frame-to-frame changes (process noise estimate)
 * 
 * @param line_data The LineData containing lines
 * @param ground_truth Map of ground truth assignments
 * @param feature_extractor The feature extractor to analyze
 * @param feature_name Name of the feature for logging
 * @return Statistics computed from ground truth data
 */
template<typename FeatureExtractor>
FeatureStatistics analyzeGroundTruthFeatureStatistics(
        std::shared_ptr<LineData> const& line_data,
        StateEstimation::Tracker<Line2D>::GroundTruthMap const& ground_truth,
        FeatureExtractor const& feature_extractor,
        std::string const& feature_name) {
    
    FeatureStatistics stats;
    
    // Collect all feature values and frame-to-frame changes per group
    std::map<GroupId, std::vector<double>> group_feature_values;
    
    for (auto const& [time, group_assignments] : ground_truth) {
        for (auto const& [group_id, entity_id] : group_assignments) {
            // Get the line for this entity
            auto line = line_data->getLineByEntityId(entity_id);
            if (!line.has_value()) continue;  // Entity not found
            
            // Extract feature
            Eigen::VectorXd features = feature_extractor.getFilterFeatures(line.value());
            
            // For now, assume 1D features (extend later if needed)
            if (features.size() == 1) {
                group_feature_values[group_id].push_back(features(0));
            }
        }
    }
    
    // Compute statistics
    std::vector<double> all_values;
    std::vector<double> all_changes;
    
    for (auto const& [group_id, values] : group_feature_values) {
        if (values.empty()) continue;
        
        // Collect all values
        all_values.insert(all_values.end(), values.begin(), values.end());
        
        // Compute frame-to-frame changes
        for (size_t i = 1; i < values.size(); ++i) {
            double change = std::abs(values[i] - values[i-1]);
            all_changes.push_back(change);
        }
    }
    
    if (all_values.empty()) {
        return stats;  // No data
    }
    
    // Compute mean
    stats.mean = std::accumulate(all_values.begin(), all_values.end(), 0.0) / all_values.size();
    stats.num_samples = static_cast<int>(all_values.size());
    
    // Compute variance
    double sum_sq_diff = 0.0;
    for (double val : all_values) {
        double diff = val - stats.mean;
        sum_sq_diff += diff * diff;
    }
    stats.variance = sum_sq_diff / all_values.size();
    stats.std_dev = std::sqrt(stats.variance);
    
    // Compute frame-to-frame change statistics (process noise estimate)
    if (!all_changes.empty()) {
        stats.mean_frame_to_frame_change = std::accumulate(all_changes.begin(), all_changes.end(), 0.0) / all_changes.size();
        stats.num_transitions = static_cast<int>(all_changes.size());
        
        double sum_sq_change_diff = 0.0;
        for (double change : all_changes) {
            double diff = change - stats.mean_frame_to_frame_change;
            sum_sq_change_diff += diff * diff;
        }
        stats.variance_frame_to_frame_change = sum_sq_change_diff / all_changes.size();
    }
    
    return stats;
}

} // anonymous namespace



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
    
    // Create composite feature extractor with centroid + base point + length
    // Uses metadata-driven approach to handle different feature types
    // - Centroid & base point: KINEMATIC_2D (position + velocity)
    // - Length: STATIC (no velocity tracking)
    auto composite_extractor = std::make_unique<StateEstimation::CompositeFeatureExtractor<Line2D>>();
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineCentroidExtractor>());
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineBasePointExtractor>());
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineLengthExtractor>());
    
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
    
    // Auto-estimate noise parameters from ground truth data if requested
    double estimated_length_process_noise_scale = params->static_feature_process_noise_scale;
    double estimated_length_measurement_noise = params->measurement_noise_length;
    
    if (params->auto_estimate_static_noise || params->auto_estimate_measurement_noise) {
        StateEstimation::LineLengthExtractor length_extractor;
        auto length_stats = analyzeGroundTruthFeatureStatistics(
            line_data, ground_truth, length_extractor, "line_length");
        
        if (length_stats.num_samples > 0) {
            if (params->verbose_output) {
                std::cout << "\n=== Ground Truth Length Statistics ===" << std::endl;
                std::cout << "Samples: " << length_stats.num_samples << std::endl;
                std::cout << "Mean length: " << length_stats.mean << " pixels" << std::endl;
                std::cout << "Std dev: " << length_stats.std_dev << " pixels" << std::endl;
                std::cout << "Frame-to-frame changes: " << length_stats.num_transitions << " transitions" << std::endl;
                std::cout << "Mean absolute change: " << length_stats.mean_frame_to_frame_change << " pixels/frame" << std::endl;
                std::cout << "Std dev of changes: " << std::sqrt(length_stats.variance_frame_to_frame_change) << " pixels/frame" << std::endl;
            }
            
            if (params->auto_estimate_static_noise && length_stats.num_transitions > 0) {
                // Use the observed frame-to-frame variance as basis for process noise
                // Apply the percentile scaling (e.g., 10% of observed variation)
                double observed_change_variance = length_stats.variance_frame_to_frame_change;
                
                // Scale: we want Q = (percentile × change_std_dev)²
                // But Q is scaled by static_noise_scale × position_var
                // So: static_noise_scale × position_var² = (percentile × change_std_dev)²
                // Therefore: static_noise_scale = (percentile × change_std_dev)² / position_var²
                
                double change_std_dev = std::sqrt(observed_change_variance);
                double target_process_std = params->static_noise_percentile * change_std_dev;
                
                estimated_length_process_noise_scale = 
                    (target_process_std * target_process_std) / 
                    (params->process_noise_position * params->process_noise_position);
                
                if (params->verbose_output) {
                    std::cout << "\nAuto-estimated static noise:" << std::endl;
                    std::cout << "  Target process std dev: " << target_process_std << " pixels/frame" << std::endl;
                    std::cout << "  Computed scale factor: " << estimated_length_process_noise_scale << std::endl;
                    std::cout << "  (was: " << params->static_feature_process_noise_scale << ")" << std::endl;
                }
            }
            
            if (params->auto_estimate_measurement_noise) {
                // Use the percentile of the overall standard deviation as measurement noise
                estimated_length_measurement_noise = params->static_noise_percentile * length_stats.std_dev;
                
                if (params->verbose_output) {
                    std::cout << "\nAuto-estimated measurement noise:" << std::endl;
                    std::cout << "  Estimated: " << estimated_length_measurement_noise << " pixels" << std::endl;
                    std::cout << "  (was: " << params->measurement_noise_length << ")" << std::endl;
                }
            }
        } else if (params->verbose_output) {
            std::cout << "\nWarning: No ground truth data found for noise estimation" << std::endl;
        }
    }
    
    // Build Kalman matrices from metadata with per-feature noise configuration
    // This automatically creates correct block-diagonal structure
    StateEstimation::KalmanMatrixBuilder::PerFeatureConfig config;
    config.dt = params->dt;
    config.process_noise_position = params->process_noise_position;
    config.process_noise_velocity = params->process_noise_velocity;
    config.static_noise_scale = estimated_length_process_noise_scale;  // Use estimated or default
    config.measurement_noise = params->measurement_noise_position;     // Default for position features
    
    // Set feature-specific measurement noise
    config.feature_measurement_noise["line_centroid"] = params->measurement_noise_position;
    config.feature_measurement_noise["line_base_point"] = params->measurement_noise_position;
    config.feature_measurement_noise["line_length"] = estimated_length_measurement_noise;  // Use estimated or default
    
    auto [F, H, Q, R] = StateEstimation::KalmanMatrixBuilder::buildAllMatricesFromMetadataPerFeature(
        metadata_list, config);
    
    if (params->verbose_output) {
        std::cout << "\nNoise configuration:" << std::endl;
        std::cout << "  Process noise - position: " << params->process_noise_position << std::endl;
        std::cout << "  Process noise - velocity: " << params->process_noise_velocity << std::endl;
        std::cout << "  Process noise - static scale: " << params->static_feature_process_noise_scale << std::endl;
        std::cout << "  Measurement noise - position: " << params->measurement_noise_position << std::endl;
        std::cout << "  Measurement noise - length: " << params->measurement_noise_length << std::endl;
        std::cout << "\nResulting Q (process noise covariance) diagonal:" << std::endl;
        for (int i = 0; i < Q.rows(); ++i) {
            std::cout << "    Q[" << i << "," << i << "] = " << Q(i, i) << std::endl;
        }
        std::cout << "\nResulting R (measurement noise covariance) diagonal:" << std::endl;
        for (int i = 0; i < R.rows(); ++i) {
            std::cout << "    R[" << i << "," << i << "] = " << R(i, i) << std::endl;
        }
    }
    
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
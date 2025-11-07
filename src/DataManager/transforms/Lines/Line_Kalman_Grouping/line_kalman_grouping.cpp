#include "line_kalman_grouping.hpp"

#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "StateEstimation/DataAdapter.hpp"
#include "StateEstimation/Features/CompositeFeatureExtractor.hpp"
#include "StateEstimation/Features/IFeatureExtractor.hpp"
#include "StateEstimation/Features/LineBasePointExtractor.hpp"
#include "StateEstimation/Features/LineCentroidExtractor.hpp"
#include "StateEstimation/Features/LineLengthExtractor.hpp"
#include "StateEstimation/Filter/Kalman/KalmanFilter.hpp"
#include "StateEstimation/Filter/Kalman/KalmanMatrixBuilder.hpp"
#include "StateEstimation/MinCostFlowTracker.hpp"
#include "transforms/utils/variant_type_check.hpp"

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
 * @brief Cross-correlation statistics between two features
 */
struct CrossCorrelationStatistics {
    double pearson_correlation = 0.0;// Pearson correlation coefficient (-1 to 1)
    int num_paired_samples = 0;
    bool is_valid = false;
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
        std::shared_ptr<LineData> const & line_data,
        std::map<TimeFrameIndex, std::map<GroupId, EntityId>> const & ground_truth,
        FeatureExtractor const & feature_extractor,
        std::string const & feature_name) {
    (void) feature_name;

    FeatureStatistics stats;

    // Collect all feature values and frame-to-frame changes per group
    std::map<GroupId, std::vector<double>> group_feature_values;

    for (auto const & [time, group_assignments]: ground_truth) {
        for (auto const & [group_id, entity_id]: group_assignments) {
            // Get the line for this entity
            auto line = line_data->getDataByEntityId(entity_id);
            if (!line.has_value()) continue;// Entity not found

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

    for (auto const & [group_id, values]: group_feature_values) {
        if (values.empty()) continue;

        // Collect all values
        all_values.insert(all_values.end(), values.begin(), values.end());

        // Compute frame-to-frame changes
        for (size_t i = 1; i < values.size(); ++i) {
            double change = std::abs(values[i] - values[i - 1]);
            all_changes.push_back(change);
        }
    }

    if (all_values.empty()) {
        return stats;// No data
    }

    // Compute mean
    stats.mean = std::accumulate(all_values.begin(), all_values.end(), 0.0) / all_values.size();
    stats.num_samples = static_cast<int>(all_values.size());

    // Compute variance
    double sum_sq_diff = 0.0;
    for (double val: all_values) {
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
        for (double change: all_changes) {
            double diff = change - stats.mean_frame_to_frame_change;
            sum_sq_change_diff += diff * diff;
        }
        stats.variance_frame_to_frame_change = sum_sq_change_diff / all_changes.size();
    }

    return stats;
}

/**
 * @brief Compute empirical correlation between two features from ground truth data
 * 
 * Uses Pearson correlation coefficient to measure linear relationship between features.
 * This is computed from actual observed data, not assumptions.
 * 
 * @param line_data The LineData containing lines
 * @param ground_truth Map of ground truth assignments
 * @param extractor_a First feature extractor
 * @param extractor_b Second feature extractor
 * @param feature_a_name Name of first feature (for logging)
 * @param feature_b_name Name of second feature (for logging)
 * @return Cross-correlation statistics
 */
template<typename ExtractorA, typename ExtractorB>
CrossCorrelationStatistics computeFeatureCrossCorrelation(
        std::shared_ptr<LineData> const & line_data,
        std::map<TimeFrameIndex, std::map<GroupId, EntityId>> const & ground_truth,
        ExtractorA const & extractor_a,
        ExtractorB const & extractor_b,
        std::string const & feature_a_name,
        std::string const & feature_b_name) {
    (void) feature_a_name;
    (void) feature_b_name;

    CrossCorrelationStatistics stats;

    // Collect paired feature values across all groups and times
    std::vector<double> values_a, values_b;

    for (auto const & [time, group_assignments]: ground_truth) {
        for (auto const & [group_id, entity_id]: group_assignments) {
            auto line = line_data->getDataByEntityId(entity_id);
            if (!line.has_value()) continue;

            // Extract both features
            Eigen::VectorXd feat_a = extractor_a.getFilterFeatures(line.value());
            Eigen::VectorXd feat_b = extractor_b.getFilterFeatures(line.value());

            // For multi-dimensional features, use first component or magnitude
            double val_a = (feat_a.size() == 1) ? feat_a(0) : feat_a.norm();
            double val_b = (feat_b.size() == 1) ? feat_b(0) : feat_b.norm();

            values_a.push_back(val_a);
            values_b.push_back(val_b);
        }
    }

    if (values_a.size() < 3) {
        return stats;// Not enough data for meaningful correlation
    }

    stats.num_paired_samples = static_cast<int>(values_a.size());

    // Compute means
    double mean_a = std::accumulate(values_a.begin(), values_a.end(), 0.0) / values_a.size();
    double mean_b = std::accumulate(values_b.begin(), values_b.end(), 0.0) / values_b.size();

    // Compute covariance and standard deviations
    double cov_ab = 0.0;
    double var_a = 0.0;
    double var_b = 0.0;

    for (size_t i = 0; i < values_a.size(); ++i) {
        double diff_a = values_a[i] - mean_a;
        double diff_b = values_b[i] - mean_b;

        cov_ab += diff_a * diff_b;
        var_a += diff_a * diff_a;
        var_b += diff_b * diff_b;
    }

    cov_ab /= values_a.size();
    var_a /= values_a.size();
    var_b /= values_b.size();

    // Compute Pearson correlation: ρ = cov(A,B) / (σ_A × σ_B)
    double std_a = std::sqrt(var_a);
    double std_b = std::sqrt(var_b);

    if (std_a > 1e-10 && std_b > 1e-10) {
        stats.pearson_correlation = cov_ab / (std_a * std_b);
        stats.is_valid = true;
    }

    return stats;
}

}// anonymous namespace


std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params) {
    // No-op progress callback
    return ::lineKalmanGrouping(std::move(line_data), params, [](int) { /* no progress reporting */ });
}

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params,
                                             ProgressCallback const & progressCallback) {
    if (!line_data || !params) {
        return line_data;
    }

    // Check if group manager is valid (required for grouping operations)
    if (!params->hasValidGroupManager()) {
        std::cerr << "lineKalmanGrouping: EntityGroupManager is required but not set. Call setGroupManager() on parameters before execution." << std::endl;
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
    auto line_entries_range = line_data->getAllEntries();
    auto data_source = StateEstimation::flattenLineData(line_entries_range);

    if (params->verbose_output) {
        std::cout << "Created zero-copy data source from LineData" << std::endl;
    }

    // Build GroundTruthMap: frames where entities are already grouped
    std::map<TimeFrameIndex, std::map<GroupId, EntityId>> ground_truth;
    auto all_group_ids = group_manager->getAllGroupIds();

    for (auto group_id: all_group_ids) {
        auto const & entities_in_group = group_manager->getEntitiesInGroup(group_id);

        for (auto entity_id: entities_in_group) {
            // Find which frame this entity belongs to
            auto time = line_data->getTimeByEntityId(entity_id);
            if (time.has_value()) {
                ground_truth[time.value()][group_id] = entity_id;
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

    // Auto-estimate cross-feature correlations from ground truth data if requested
    std::map<std::pair<int, int>, double> estimated_correlations;

    if (params->enable_cross_feature_covariance && !ground_truth.empty()) {
        if (params->verbose_output) {
            std::cout << "\n=== Auto-Estimating Cross-Feature Correlations ===" << std::endl;
        }

        // Create extractors for correlation analysis
        StateEstimation::LineCentroidExtractor centroid_extractor;
        StateEstimation::LineBasePointExtractor base_point_extractor;
        StateEstimation::LineLengthExtractor length_extractor;

        // Compute centroid-length correlation
        auto centroid_length_corr = computeFeatureCrossCorrelation(
                line_data, ground_truth, centroid_extractor, length_extractor,
                "centroid", "length");

        // Compute base_point-length correlation
        auto base_point_length_corr = computeFeatureCrossCorrelation(
                line_data, ground_truth, base_point_extractor, length_extractor,
                "base_point", "length");

        if (params->verbose_output) {
            std::cout << "Centroid-Length correlation: " << centroid_length_corr.pearson_correlation
                      << " (n=" << centroid_length_corr.num_paired_samples << ")" << std::endl;
            std::cout << "BasePoint-Length correlation: " << base_point_length_corr.pearson_correlation
                      << " (n=" << base_point_length_corr.num_paired_samples << ")" << std::endl;
        }

        // Apply correlations above threshold
        // Feature indices: 0 = centroid, 1 = base_point, 2 = length
        if (centroid_length_corr.is_valid &&
            std::abs(centroid_length_corr.pearson_correlation) >= params->min_correlation_threshold) {
            estimated_correlations[{0, 2}] = centroid_length_corr.pearson_correlation;
            if (params->verbose_output) {
                std::cout << "  → Using centroid-length correlation: "
                          << centroid_length_corr.pearson_correlation << std::endl;
            }
        }

        if (base_point_length_corr.is_valid &&
            std::abs(base_point_length_corr.pearson_correlation) >= params->min_correlation_threshold) {
            estimated_correlations[{1, 2}] = base_point_length_corr.pearson_correlation;
            if (params->verbose_output) {
                std::cout << "  → Using base_point-length correlation: "
                          << base_point_length_corr.pearson_correlation << std::endl;
            }
        }

        if (estimated_correlations.empty() && params->verbose_output) {
            std::cout << "  → No significant correlations found (all below threshold "
                      << params->min_correlation_threshold << ")" << std::endl;
        }
    }

    // Configure cross-feature covariance in composite extractor
    if (!estimated_correlations.empty()) {
        StateEstimation::CompositeFeatureExtractor<Line2D>::CrossCovarianceConfig cross_cov_config;
        cross_cov_config.feature_correlations = estimated_correlations;
        composite_extractor->setCrossCovarianceConfig(std::move(cross_cov_config));

        if (params->verbose_output) {
            std::cout << "Configured initial cross-feature covariance from empirical correlations" << std::endl;
        }
    }

    // Get metadata from all child extractors
    // This automatically handles different temporal behaviors (kinematic, static, etc.)
    auto metadata_list = composite_extractor->getChildMetadata();

    if (params->verbose_output) {
        std::cout << "Building Kalman filter for " << metadata_list.size() << " features:" << std::endl;
        int total_meas = 0, total_state = 0;
        for (auto const & meta: metadata_list) {
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
                // Clamp to a small positive floor for numerical stability
                double constexpr kMinMeasNoise = 1.0;// pixels
                if (estimated_length_measurement_noise < kMinMeasNoise) {
                    estimated_length_measurement_noise = kMinMeasNoise;
                }

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
    config.static_noise_scale = estimated_length_process_noise_scale;// Use estimated or default
    config.measurement_noise = params->measurement_noise_position;   // Default for position features

    // Set feature-specific measurement noise
    config.feature_measurement_noise["line_centroid"] = params->measurement_noise_position;
    config.feature_measurement_noise["line_base_point"] = params->measurement_noise_position;
    config.feature_measurement_noise["line_length"] = estimated_length_measurement_noise;// Use estimated or default

    auto [F, H, Q, R] = StateEstimation::KalmanMatrixBuilder::buildAllMatricesFromMetadataPerFeature(
            metadata_list, config);

    // Add cross-feature process noise using estimated correlations
    if (!estimated_correlations.empty()) {
        Q = StateEstimation::KalmanMatrixBuilder::addCrossFeatureProcessNoise(
                Q, metadata_list, estimated_correlations);

        if (params->verbose_output) {
            std::cout << "\nAdded cross-feature process noise covariance based on empirical correlations" << std::endl;
        }
    }

    if (params->verbose_output) {
        std::cout << "\nNoise configuration:" << std::endl;
        std::cout << "  Process noise - position: " << params->process_noise_position << std::endl;
        std::cout << "  Process noise - velocity: " << params->process_noise_velocity << std::endl;
        std::cout << "  Process noise - static scale: " << estimated_length_process_noise_scale;
        if (params->auto_estimate_static_noise) {
            std::cout << " (auto-estimated, parameter was: " << params->static_feature_process_noise_scale << ")";
        }
        std::cout << std::endl;
        std::cout << "  Measurement noise - position: " << params->measurement_noise_position << std::endl;
        std::cout << "  Measurement noise - length: " << estimated_length_measurement_noise;
        if (params->auto_estimate_measurement_noise) {
            std::cout << " (auto-estimated, parameter was: " << params->measurement_noise_length << ")";
        }
        std::cout << std::endl;
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

    // Build a state index map for dynamics-aware costs (order-independent)
    auto index_map = StateEstimation::KalmanMatrixBuilder::buildStateIndexMap(metadata_list);

    // Create dynamics-aware transition cost (measurement NLL + velocity + implied-acceleration)
    auto cost_fn = StateEstimation::createDynamicsAwareCostFunction(
            H,
            R,
            index_map,
            config.dt,
            /*beta=*/1.0,
            /*gamma=*/0.25,
            /*lambda_gap=*/0.0);

    // Use MinCostFlowTracker with the custom cost function
    // Relax greedy cheap-link threshold to account for added dynamics terms
    double const cheap_threshold = params->cheap_assignment_threshold * 5.0;
    // Use Mahalanobis for greedy chaining and dynamics-aware for transitions
    auto chain_cost = StateEstimation::createMahalanobisCostFunction(H, R);
    StateEstimation::MinCostFlowTracker<Line2D> tracker(
            std::move(kalman_filter),
            std::move(composite_extractor),
            chain_cost,
            cost_fn,
            params->cost_scale_factor,
            cheap_threshold);

    if (params->verbose_output) {
        tracker.enableDebugLogging("tracker.log");
    }

    // Build group -> sorted anchor frames mapping
    std::map<GroupId, std::vector<TimeFrameIndex>> group_to_anchor_frames;
    for (auto const & [frame, assignments]: ground_truth) {
        for (auto const & [group_id, _]: assignments) {
            group_to_anchor_frames[group_id].push_back(frame);
        }
    }

    for (auto & [gid, frames]: group_to_anchor_frames) {
        std::sort(frames.begin(), frames.end());
        frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
    }

    // Count total groups we will process (with at least two anchors)
    size_t total_groups_to_process = 0;
    for (auto const & [gid, frames]: group_to_anchor_frames) {
        if (frames.size() > 1) total_groups_to_process += 1;
    }
    size_t processed_groups = 0;

    if (params->verbose_output) {
        std::cout << "\nProcessing per-group anchors across " << group_to_anchor_frames.size() << " groups" << std::endl;
    }

    for (auto const & [group_id, frames]: group_to_anchor_frames) {
        if (frames.size() < 2) continue;

        // Create putative output group for this anchor group if requested
        std::optional<GroupId> putative_group_id;
        if (params->write_to_putative_groups) {
            auto desc = group_manager->getGroupDescriptor(group_id);
            std::string base_name = desc ? desc->name : std::string("Group ") + std::to_string(group_id);
            std::string putative_name = params->putative_group_prefix + base_name;
            putative_group_id = group_manager->createGroup(putative_name, "Putative labels from Kalman grouping");
        }

        // Solve once across the full anchor span: first -> last
        TimeFrameIndex interval_start = frames.front();
        TimeFrameIndex interval_end = frames.back();

        // Build ground truth map including ALL anchors for this group across the full span
        std::map<TimeFrameIndex, std::map<GroupId, EntityId>> gt_local;
        for (auto const & f: frames) {
            auto it_frame = ground_truth.find(f);
            if (it_frame == ground_truth.end()) continue;
            auto const & fmap = it_frame->second;
            auto it_gid = fmap.find(group_id);
            if (it_gid != fmap.end()) {
                gt_local[f][group_id] = it_gid->second;
            }
        }
        // Safety: require anchors at both ends of the interval
        if (gt_local.count(interval_start) == 0 || gt_local.count(interval_end) == 0) {
            continue;
        }

        if (params->verbose_output) {
            std::cout << "\nProcessing group " << group_id << " full span: "
                      << interval_start.getValue() << " -> " << interval_end.getValue() << std::endl;
        }

        // Exclude already-labeled entities from matching; allow anchors explicitly
        std::unordered_set<EntityId> excluded_entities;
        for (auto gid: all_group_ids) {
            auto ents = group_manager->getEntitiesInGroup(gid);
            excluded_entities.insert(ents.begin(), ents.end());
        }
        std::unordered_set<EntityId> include_entities;// whitelist anchors at ends
        include_entities.insert(gt_local[interval_start][group_id]);
        include_entities.insert(gt_local[interval_end][group_id]);

        // Map write group
        std::map<GroupId, GroupId> write_group_map;
        if (putative_group_id.has_value()) {
            write_group_map[group_id] = *putative_group_id;
        }

        [[maybe_unused]] auto smoothed_results = tracker.process(
                data_source,
                *group_manager,
                gt_local,
                interval_start,
                interval_end,
                progressCallback,
                putative_group_id.has_value() ? &write_group_map : nullptr,
                nullptr,
                nullptr);

        // After completing this group, notify and update progress
        group_manager->notifyGroupsChanged();
        processed_groups++;
        int progress = total_groups_to_process > 0 ? static_cast<int>(100.0 * processed_groups / total_groups_to_process) : 100;
        progressCallback(progress);
    }

    if (params->verbose_output) {
        std::cout << "Tracking complete. Groups updated in EntityGroupManager." << std::endl;
        for (auto group_id: all_group_ids) {
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

bool LineKalmanGroupingOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineKalmanGroupingOperation::getDefaultParameters() const {
    // Create default parameters with null group manager
    // The EntityGroupManager must be set via setGroupManager() before execution
    return std::make_unique<LineKalmanGroupingParameters>();
}

DataTypeVariant LineKalmanGroupingOperation::execute(DataTypeVariant const & dataVariant,
                                                     TransformParametersBase const * transformParameters) {
    // No-op progress callback
    return execute(dataVariant, transformParameters, [](int) { /* no progress reporting */ });
}

DataTypeVariant LineKalmanGroupingOperation::execute(DataTypeVariant const & dataVariant,
                                                     TransformParametersBase const * transformParameters,
                                                     ProgressCallback progressCallback) {
    if (!canApply(dataVariant)) {
        return DataTypeVariant{};
    }

    auto line_data = std::get<std::shared_ptr<LineData>>(dataVariant);
    auto params = dynamic_cast<LineKalmanGroupingParameters const *>(transformParameters);

    if (!params) {
        return DataTypeVariant{};
    }

    // Check if group manager is valid (required for grouping operations)
    if (!params->hasValidGroupManager()) {
        std::cerr << "LineKalmanGroupingOperation::execute: EntityGroupManager is required but not set. Call setGroupManager() on parameters before execution." << std::endl;
        return DataTypeVariant{};
    }

    auto result = ::lineKalmanGrouping(line_data, params, progressCallback);
    return DataTypeVariant{result};
}
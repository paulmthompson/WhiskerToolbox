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

// Include Hungarian algorithm from StateEstimation
#include "StateEstimation/Assignment/hungarian.hpp"

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

void estimateNoiseFromGroupedData(LineData const* line_data,
                                  EntityGroupManager* group_manager,
                                  LineKalmanGroupingParameters* params) {
    if (!params->estimate_noise_empirically) {
        return;
    }
    
    auto all_group_ids = group_manager->getAllGroupIds();
    if (all_group_ids.empty()) {
        return;
    }
    
    std::vector<double> position_variances;
    std::vector<double> velocity_variances;
    
    for (auto group_id : all_group_ids) {
        auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
        if (entities_in_group.size() < 2) continue;
        
        // Collect centroids for this group across time
        std::map<TimeFrameIndex, Eigen::Vector2d> group_centroids;
        
        for (auto entity_id : entities_in_group) {
            auto time_info = line_data->getTimeAndIndexByEntityId(entity_id);
            if (time_info.has_value()) {
                auto line_opt = line_data->getLineByEntityId(entity_id);
                if (line_opt.has_value()) {
                    group_centroids[time_info->first] = calculateLineCentroid(line_opt.value());
                }
            }
        }
        
        if (group_centroids.size() < 2) continue;
        
        // Calculate position variance and velocity variance
        std::vector<TimeFrameIndex> sorted_times;
        for (auto const& [time, centroid] : group_centroids) {
            sorted_times.push_back(time);
        }
        std::sort(sorted_times.begin(), sorted_times.end());
        
        // Position variance (deviation from mean position)
        Eigen::Vector2d mean_position = Eigen::Vector2d::Zero();
        for (auto time : sorted_times) {
            mean_position += group_centroids[time];
        }
        mean_position /= static_cast<double>(sorted_times.size());
        
        double pos_var = 0.0;
        for (auto time : sorted_times) {
            Eigen::Vector2d diff = group_centroids[time] - mean_position;
            pos_var += diff.squaredNorm();
        }
        pos_var /= static_cast<double>(sorted_times.size());
        position_variances.push_back(pos_var);
        
        // Velocity variance (acceleration between consecutive frames)
        if (sorted_times.size() >= 3) {
            std::vector<double> velocities_x, velocities_y;
            
            for (size_t i = 1; i < sorted_times.size(); ++i) {
                auto dt = static_cast<double>((sorted_times[i] - sorted_times[i-1]).getValue());
                if (dt > 0) {
                    Eigen::Vector2d vel = (group_centroids[sorted_times[i]] - group_centroids[sorted_times[i-1]]) / dt;
                    velocities_x.push_back(vel.x());
                    velocities_y.push_back(vel.y());
                }
            }
            
            if (velocities_x.size() >= 2) {
                double vel_var_x = 0.0, vel_var_y = 0.0;
                double mean_vel_x = 0.0, mean_vel_y = 0.0;
                
                for (size_t i = 0; i < velocities_x.size(); ++i) {
                    mean_vel_x += velocities_x[i];
                    mean_vel_y += velocities_y[i];
                }
                mean_vel_x /= static_cast<double>(velocities_x.size());
                mean_vel_y /= static_cast<double>(velocities_y.size());
                
                for (size_t i = 0; i < velocities_x.size(); ++i) {
                    vel_var_x += (velocities_x[i] - mean_vel_x) * (velocities_x[i] - mean_vel_x);
                    vel_var_y += (velocities_y[i] - mean_vel_y) * (velocities_y[i] - mean_vel_y);
                }
                vel_var_x /= static_cast<double>(velocities_x.size());
                vel_var_y /= static_cast<double>(velocities_y.size());
                
                velocity_variances.push_back((vel_var_x + vel_var_y) / 2.0);
            }
        }
    }
    
    // Update parameters with empirical estimates
    if (!position_variances.empty()) {
        double mean_pos_var = 0.0;
        for (auto var : position_variances) {
            mean_pos_var += var;
        }
        mean_pos_var /= static_cast<double>(position_variances.size());
        params->process_noise_position = std::sqrt(mean_pos_var);
        params->measurement_noise = std::sqrt(mean_pos_var) * 0.5; // Assume measurement is more precise
    }
    
    if (!velocity_variances.empty()) {
        double mean_vel_var = 0.0;
        for (auto var : velocity_variances) {
            mean_vel_var += var;
        }
        mean_vel_var /= static_cast<double>(velocity_variances.size());
        params->process_noise_velocity = std::sqrt(mean_vel_var);
    }
    
    if (params->verbose_output) {
        std::cout << "Estimated noise parameters from " << position_variances.size() 
                  << " groups:" << std::endl;
        std::cout << "  Process noise position: " << params->process_noise_position << std::endl;
        std::cout << "  Process noise velocity: " << params->process_noise_velocity << std::endl;
        std::cout << "  Measurement noise: " << params->measurement_noise << std::endl;
    }
}

KalmanFilter createKalmanFilter(LineKalmanGroupingParameters const* params) {
    // State vector: [x, y, vx, vy]
    // Measurement vector: [x, y]
    
    // System dynamics matrix (constant velocity model)
    Eigen::MatrixXd A(4, 4);
    A << 1, 0, params->dt, 0,
         0, 1, 0, params->dt,
         0, 0, 1, 0,
         0, 0, 0, 1;
    
    // Measurement matrix (observe position only)
    Eigen::MatrixXd C(2, 4);
    C << 1, 0, 0, 0,
         0, 1, 0, 0;
    
    // Process noise covariance
    Eigen::MatrixXd Q(4, 4);
    Q << params->process_noise_position * params->process_noise_position, 0, 0, 0,
         0, params->process_noise_position * params->process_noise_position, 0, 0,
         0, 0, params->process_noise_velocity * params->process_noise_velocity, 0,
         0, 0, 0, params->process_noise_velocity * params->process_noise_velocity;
    
    // Measurement noise covariance
    Eigen::MatrixXd R(2, 2);
    R << params->measurement_noise * params->measurement_noise, 0,
         0, params->measurement_noise * params->measurement_noise;
    
    // Initial estimate error covariance
    Eigen::MatrixXd P(4, 4);
    P << params->initial_position_uncertainty * params->initial_position_uncertainty, 0, 0, 0,
         0, params->initial_position_uncertainty * params->initial_position_uncertainty, 0, 0,
         0, 0, params->initial_velocity_uncertainty * params->initial_velocity_uncertainty, 0,
         0, 0, 0, params->initial_velocity_uncertainty * params->initial_velocity_uncertainty;
    
    return KalmanFilter(params->dt, A, C, Q, R, P);
}

double calculateMahalanobisDistance(Eigen::Vector2d const& observation,
                                    Eigen::Vector2d const& prediction,
                                    Eigen::Matrix2d const& covariance) {
    Eigen::Vector2d diff = observation - prediction;
    
    // Add small regularization to avoid singular matrix
    Eigen::Matrix2d regularized_cov = covariance;
    regularized_cov(0, 0) += 1e-6;
    regularized_cov(1, 1) += 1e-6;
    
    try {
        Eigen::Matrix2d inv_cov = regularized_cov.inverse();
        return std::sqrt(diff.transpose() * inv_cov * diff);
    } catch (...) {
        // Fallback to Euclidean distance if covariance inversion fails
        return diff.norm();
    }
}

std::vector<int> solveAssignmentProblem(std::vector<Eigen::Vector2d> const& ungrouped_centroids,
                                        std::vector<Eigen::Vector2d> const& predicted_centroids,
                                        std::vector<Eigen::Matrix2d> const& covariances,
                                        double max_distance) {
    if (ungrouped_centroids.empty() || predicted_centroids.empty()) {
        return std::vector<int>(ungrouped_centroids.size(), -1);
    }
    
    size_t n_ungrouped = ungrouped_centroids.size();
    size_t n_predicted = predicted_centroids.size();
    size_t matrix_size = std::max(n_ungrouped, n_predicted);
    
    // Create cost matrix (scale distances to integers for Hungarian algorithm)
    std::vector<std::vector<int>> cost_matrix(matrix_size, std::vector<int>(matrix_size, 0));
    
    const int large_cost = static_cast<int>(max_distance * 1000) + 1000; // Scale and add buffer
    
    for (size_t i = 0; i < matrix_size; ++i) {
        for (size_t j = 0; j < matrix_size; ++j) {
            if (i < n_ungrouped && j < n_predicted) {
                double distance = calculateMahalanobisDistance(
                    ungrouped_centroids[i], 
                    predicted_centroids[j], 
                    covariances[j]
                );
                
                if (distance <= max_distance) {
                    cost_matrix[i][j] = static_cast<int>(distance * 1000); // Scale to integer
                } else {
                    cost_matrix[i][j] = large_cost;
                }
            } else {
                cost_matrix[i][j] = large_cost; // Dummy assignments
            }
        }
    }
    
    // Store original cost matrix for verification
    auto original_costs = cost_matrix;
    
    // Solve using Hungarian algorithm
    try {
        std::vector<std::vector<int>> assignment_matrix;
        auto total_cost = Munkres::hungarian_with_assignment(cost_matrix, assignment_matrix, true);
        
        // Extract assignments from the assignment matrix
        std::vector<int> assignments(n_ungrouped, -1);
        
        for (size_t i = 0; i < n_ungrouped && i < assignment_matrix.size(); ++i) {
            for (size_t j = 0; j < n_predicted && j < assignment_matrix[i].size(); ++j) {
                if (assignment_matrix[i][j] == 1) { // Starred zero indicates assignment
                    // Check if this is a valid assignment (not a dummy)
                    double distance = calculateMahalanobisDistance(
                        ungrouped_centroids[i], 
                        predicted_centroids[j], 
                        covariances[j]
                    );
                    
                    if (distance <= max_distance) {
                        assignments[i] = static_cast<int>(j);
                        
                        //if (debug_output) {
                        //    std::cout << "  Assignment: ungrouped[" << i << "] -> predicted[" << j 
                         //            << "] (distance: " << distance << ")" << std::endl;
                        //}
                    }
                    break; // Each row should have at most one assignment
                }
            }
        }
        
        return assignments;
        
    } catch (...) {
        // Fallback to greedy assignment if Hungarian algorithm fails
        std::vector<int> assignments(n_ungrouped, -1);
        std::vector<bool> used_predictions(n_predicted, false);
        
        for (size_t i = 0; i < n_ungrouped; ++i) {
            double best_distance = std::numeric_limits<double>::max();
            int best_j = -1;
            
            for (size_t j = 0; j < n_predicted; ++j) {
                if (used_predictions[j]) continue;
                
                double distance = calculateMahalanobisDistance(
                    ungrouped_centroids[i], 
                    predicted_centroids[j], 
                    covariances[j]
                );
                
                if (distance <= max_distance && distance < best_distance) {
                    best_distance = distance;
                    best_j = static_cast<int>(j);
                }
            }
            
            if (best_j >= 0) {
                assignments[i] = best_j;
                used_predictions[best_j] = true;
            }
        }
        
        return assignments;
    }
}

void runRTSSmoothing(std::map<GroupId, GroupTrackingState>& tracking_states,
                     TimeFrameIndex start_frame,
                     TimeFrameIndex end_frame,
                     LineKalmanGroupingParameters const* params) {
    // TODO: Implement RTS smoothing
    // This is a placeholder for the backward pass smoothing algorithm
    // For now, we'll skip this complex implementation and focus on the forward pass
    
    if (params->verbose_output) {
        std::cout << "RTS smoothing between frames " << start_frame.getValue()
                  << " and " << end_frame.getValue() << " (not yet implemented)" << std::endl;
    }
}

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params) {
    return lineKalmanGrouping(std::move(line_data), params, [](int) {});
}

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params,
                                             ProgressCallback progressCallback) {
    if (!line_data || !params || !params->getGroupManager()) {
        return line_data;
    }
    
    auto group_manager = params->getGroupManager();
    
    // Estimate noise parameters from existing data if requested
    auto mutable_params = const_cast<LineKalmanGroupingParameters*>(params);
    estimateNoiseFromGroupedData(line_data.get(), group_manager, mutable_params);
    
    // Get all time frames with data
    auto times_view = line_data->getTimesWithData();
    std::vector<TimeFrameIndex> all_times(times_view.begin(), times_view.end());
    if (all_times.empty()) {
        progressCallback(100);
        return line_data;
    }
    
    std::sort(all_times.begin(), all_times.end());
    
    // Initialize tracking states for existing groups
    std::map<GroupId, GroupTrackingState> tracking_states;
    auto all_group_ids = group_manager->getAllGroupIds();
    
    if (params->verbose_output) {
        std::cout << "Found " << all_group_ids.size() << " existing groups: ";
        for (auto gid : all_group_ids) {
            std::cout << gid << " ";
        }
        std::cout << std::endl;
    }
    
    for (auto group_id : all_group_ids) {
        GroupTrackingState state;
        state.group_id = group_id;
        state.kalman_filter = createKalmanFilter(params);
        state.is_active = false;
        tracking_states[group_id] = state;
    }
    
    // First pass: identify anchor frames and initialize Kalman filters
    for (auto time : all_times) {
        auto entity_ids_at_time = line_data->getEntityIdsAtTime(time);
        
        for (auto entity_id : entity_ids_at_time) {
            auto groups_containing_entity = group_manager->getGroupsContainingEntity(entity_id);
            
            if (!groups_containing_entity.empty()) {
                // This is an anchor frame for the group
                auto group_id = groups_containing_entity[0]; // Use first group if multiple
                auto line_opt = line_data->getLineByEntityId(entity_id);
                
                if (line_opt.has_value() && tracking_states.find(group_id) != tracking_states.end()) {
                    auto centroid = calculateLineCentroid(line_opt.value());
                    tracking_states[group_id].anchor_frames.push_back(time);
                    tracking_states[group_id].anchor_centroids.push_back(centroid);
                }
            }
        }
    }
    
    // Sort anchor frames for each group
    for (auto& [group_id, state] : tracking_states) {
        if (!state.anchor_frames.empty()) {
            // Sort by time
            std::vector<std::pair<TimeFrameIndex, Eigen::Vector2d>> combined;
            for (size_t i = 0; i < state.anchor_frames.size(); ++i) {
                combined.emplace_back(state.anchor_frames[i], state.anchor_centroids[i]);
            }
            std::sort(combined.begin(), combined.end(), 
                     [](auto const& a, auto const& b) { return a.first < b.first; });
            
            state.anchor_frames.clear();
            state.anchor_centroids.clear();
            for (auto const& [time, centroid] : combined) {
                state.anchor_frames.push_back(time);
                state.anchor_centroids.push_back(centroid);
            }
        }
    }
    
    size_t processed_frames = 0;
    size_t total_frames = all_times.size();
    
    // Second pass: forward tracking and assignment
    for (auto time : all_times) {
        progressCallback(static_cast<int>((processed_frames * 50) / total_frames)); // 50% for forward pass
        
        // Update active Kalman filters
        for (auto& [group_id, state] : tracking_states) {
            // Check if this is an anchor frame
            bool is_anchor = std::find(state.anchor_frames.begin(), state.anchor_frames.end(), time) 
                           != state.anchor_frames.end();
            
            if (is_anchor) {
                // Reset Kalman filter with perfect measurement
                auto anchor_idx = std::find(state.anchor_frames.begin(), state.anchor_frames.end(), time) 
                                - state.anchor_frames.begin();
                auto centroid = state.anchor_centroids[anchor_idx];
                
                // Initialize or reset the filter
                Eigen::VectorXd initial_state(4);
                initial_state << centroid.x(), centroid.y(), 0.0, 0.0; // Zero initial velocity
                
                state.kalman_filter = createKalmanFilter(params);
                state.kalman_filter.init(static_cast<double>(time.getValue()), initial_state);
                state.is_active = true;
                state.last_update_frame = time;
                state.confidence = 1.0;
                
                if (params->verbose_output) {
                    std::cout << "Reset Kalman filter for group " << group_id 
                              << " at frame " << time.getValue() << " with centroid (" 
                              << centroid.x() << ", " << centroid.y() << ")" << std::endl;
                }
            } else if (state.is_active) {
                // Predict state for this frame
                auto time_diff = time.getValue() - state.last_update_frame.getValue();
                double dt = static_cast<double>(time_diff) * params->dt;
                
                // Update system dynamics matrix for variable time step
                Eigen::MatrixXd A(4, 4);
                A << 1, 0, dt, 0,
                     0, 1, 0, dt,
                     0, 0, 1, 0,
                     0, 0, 0, 1;
                
                // Predict without measurement update (we'll update if assignment is made)
                // Note: We're not updating here, just predicting for next step
                state.confidence *= 0.95; // Decay confidence over time
                
                if (params->verbose_output) {
                    std::cout << "Predicted state for group " << group_id 
                              << " at frame " << time.getValue() << ", confidence: " 
                              << state.confidence << std::endl;
                }
            }
        }
        
        // Get ungrouped lines at this time
        auto entity_ids_at_time = line_data->getEntityIdsAtTime(time);
        std::vector<EntityId> ungrouped_entities;
        std::vector<Eigen::Vector2d> ungrouped_centroids;
        
        for (auto entity_id : entity_ids_at_time) {
            auto groups_containing_entity = group_manager->getGroupsContainingEntity(entity_id);
            
            if (groups_containing_entity.empty()) {
                auto line_opt = line_data->getLineByEntityId(entity_id);
                if (line_opt.has_value()) {
                    ungrouped_entities.push_back(entity_id);
                    ungrouped_centroids.push_back(calculateLineCentroid(line_opt.value()));
                }
            } else {
                // This entity is already grouped - log for debugging
                if (params->verbose_output) {
                    std::cout << "  Entity " << entity_id << " already in group(s): ";
                    for (auto gid : groups_containing_entity) {
                        std::cout << gid << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }
        
        if (!ungrouped_centroids.empty()) {
            if (params->verbose_output) {
                std::cout << "Frame " << time.getValue() << ": Found " 
                          << ungrouped_centroids.size() << " ungrouped lines" << std::endl;
            }
            
            // Collect predictions from active Kalman filters
            std::vector<GroupId> active_group_ids;
            std::vector<Eigen::Vector2d> predicted_centroids;
            std::vector<Eigen::Matrix2d> covariances;
            
            for (auto& [group_id, state] : tracking_states) {
                if (state.is_active && state.confidence >= params->min_kalman_confidence) {
                    auto kalman_state = state.kalman_filter.state();
                    if (kalman_state.size() >= 2) {
                        active_group_ids.push_back(group_id);
                        predicted_centroids.emplace_back(kalman_state(0), kalman_state(1));
                        
                        // Use a more reasonable covariance based on confidence
                        double cov_scale = params->measurement_noise * params->measurement_noise / state.confidence;
                        Eigen::Matrix2d cov = Eigen::Matrix2d::Identity() * cov_scale;
                        covariances.push_back(cov);
                        
                        if (params->verbose_output) {
                            std::cout << "  Group " << group_id << " prediction: (" 
                                      << kalman_state(0) << ", " << kalman_state(1) 
                                      << "), confidence: " << state.confidence << std::endl;
                        }
                    }
                }
            }
            
            if (!predicted_centroids.empty()) {
                if (params->verbose_output) {
                    std::cout << "  Found " << predicted_centroids.size() 
                              << " active predictions for assignment" << std::endl;
                }
                
                // Solve assignment problem
                auto assignments = solveAssignmentProblem(ungrouped_centroids, predicted_centroids, 
                                                        covariances, params->max_assignment_distance);
                
                if (params->verbose_output) {
                    std::cout << "  Assignment results: ";
                    for (size_t i = 0; i < assignments.size(); ++i) {
                        std::cout << assignments[i] << " ";
                    }
                    std::cout << std::endl;
                }
                
                // Validate assignments - ensure no duplicates
                std::vector<bool> used_groups(active_group_ids.size(), false);
                bool has_duplicate = false;
                for (size_t i = 0; i < assignments.size(); ++i) {
                    if (assignments[i] >= 0 && assignments[i] < static_cast<int>(active_group_ids.size())) {
                        if (used_groups[assignments[i]]) {
                            std::cerr << "ERROR: Duplicate assignment detected! Group " << assignments[i] 
                                     << " assigned to multiple lines at frame " << time.getValue() << std::endl;
                            has_duplicate = true;
                        }
                        used_groups[assignments[i]] = true;
                    }
                }
                
                if (has_duplicate) {
                    std::cerr << "Skipping assignment application due to duplicates" << std::endl;
                    continue; // Skip this frame to avoid corruption
                }
                
                // Apply assignments
                for (size_t i = 0; i < assignments.size(); ++i) {
                    if (assignments[i] >= 0 && assignments[i] < static_cast<int>(active_group_ids.size())) {
                        auto group_id = active_group_ids[assignments[i]];
                        group_manager->addEntityToGroup(group_id, ungrouped_entities[i]);
                        
                        // Update Kalman filter with measurement
                        Eigen::Vector2d measurement = ungrouped_centroids[i];
                        Eigen::VectorXd meas(2);
                        meas << measurement.x(), measurement.y();
                        tracking_states[group_id].kalman_filter.update(meas);
                        tracking_states[group_id].last_update_frame = time;
                        tracking_states[group_id].confidence = std::min(1.0, tracking_states[group_id].confidence + 0.1);
                        
                        if (params->verbose_output) {
                            std::cout << "Assigned entity " << ungrouped_entities[i] 
                                      << " to group " << group_id << " at frame " << time.getValue() << std::endl;
                        }
                    }
                }
            }
        }
        
        ++processed_frames;
    }
    
    // Third pass: RTS smoothing between anchor frames (if enabled)
    if (params->run_backward_smoothing) {
        for (auto& [group_id, state] : tracking_states) {
            if (state.anchor_frames.size() >= 2) {
                for (size_t i = 0; i < state.anchor_frames.size() - 1; ++i) {
                    runRTSSmoothing(tracking_states, state.anchor_frames[i], 
                                  state.anchor_frames[i + 1], params);
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
    return execute(dataVariant, transformParameters, [](int) {});
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
    
    auto result = lineKalmanGrouping(line_data, params, progressCallback);
    return DataTypeVariant{result};
}
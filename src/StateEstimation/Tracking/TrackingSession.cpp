#include "TrackingSession.hpp"

#include "CoreGeometry/lines.hpp"

#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace StateEstimation {

// ========== TrackingSession Implementation ==========

TrackingSession::TrackingSession(TrackingSessionConfig config)
    : config_(std::move(config)), current_time_(0.0), next_new_group_id_(1000000) {
    
    group_tracker_ = std::make_unique<GroupTracker>(config_.kalman_config);
    
    // Default to Hungarian assignment if none provided
    if (!assignment_algorithm_) {
        assignment_algorithm_ = std::make_unique<HungarianAssignment>();
    }
}

void TrackingSession::initializeGroup(GroupId group_id, FeatureVector const& features, double time) {
    group_tracker_->initializeGroup(group_id, features, time);
    current_time_ = std::max(current_time_, time);
    
    if (config_.verbose_logging) {
        std::cout << "Initialized group " << group_id << " at time " << time << std::endl;
    }
}

TrackingUpdateResult TrackingSession::processObservations(
    std::vector<FeatureVector> const& observations,
    double time,
    std::unordered_map<std::size_t, GroupId> const& ground_truth_assignments) {
    
    TrackingUpdateResult result;
    result.success = false;
    
    if (observations.empty()) {
        result.success = true;
        return result;
    }
    
    double dt = time - current_time_;
    current_time_ = time;
    
    if (config_.verbose_logging) {
        std::cout << "Processing " << observations.size() << " observations at time " 
                  << time << " (dt=" << dt << ")" << std::endl;
    }
    
    // Handle ground truth assignments first
    std::vector<bool> observation_assigned(observations.size(), false);
    
    for (auto const& [obs_idx, group_id] : ground_truth_assignments) {
        if (obs_idx >= observations.size()) {
            continue; // Invalid observation index
        }
        
        if (!group_tracker_->isGroupTracked(group_id)) {
            // Initialize new group with ground truth
            initializeGroup(group_id, observations[obs_idx], time);
        } else {
            // Update existing group with ground truth (perfect measurement)
            group_tracker_->updateGroup(group_id, observations[obs_idx], dt);
        }
        
        observation_assigned[obs_idx] = true;
        result.updated_groups.push_back(group_id);
        
        if (config_.verbose_logging) {
            std::cout << "Ground truth: observation " << obs_idx << " -> group " << group_id << std::endl;
        }
    }
    
    // Get predictions for all tracked groups
    auto tracked_groups = group_tracker_->getTrackedGroups();
    std::vector<FeatureVector> predictions;
    std::vector<GroupId> prediction_group_ids;
    std::vector<Eigen::Matrix2d> covariances; // Simplified to 2x2 for now
    
    for (auto group_id : tracked_groups) {
        // Skip groups that already received ground truth updates
        if (std::find(result.updated_groups.begin(), result.updated_groups.end(), group_id) 
            != result.updated_groups.end()) {
            continue;
        }
        
        auto prediction = group_tracker_->predictGroup(group_id, dt);
        if (prediction.valid && prediction.confidence >= config_.confidence_threshold) {
            predictions.push_back(prediction.predicted_features);
            prediction_group_ids.push_back(group_id);
            
            // Extract 2x2 covariance for centroid (simplified)
            Eigen::Matrix2d cov = Eigen::Matrix2d::Identity() * 25.0; // Default
            if (prediction.covariance.rows() >= 2 && prediction.covariance.cols() >= 2) {
                cov = prediction.covariance.block<2,2>(0, 0);
            }
            covariances.push_back(cov);
            
            if (config_.verbose_logging) {
                std::cout << "Prediction for group " << group_id 
                          << " (confidence: " << prediction.confidence << ")" << std::endl;
            }
        } else if (config_.verbose_logging) {
            std::cout << "Skipping group " << group_id 
                      << " (confidence: " << prediction.confidence << " < " 
                      << config_.confidence_threshold << ")" << std::endl;
        }
    }
    
    // Collect unassigned observations
    std::vector<FeatureVector> unassigned_observations;
    std::vector<std::size_t> unassigned_indices;
    
    for (std::size_t i = 0; i < observations.size(); ++i) {
        if (!observation_assigned[i]) {
            unassigned_observations.push_back(observations[i]);
            unassigned_indices.push_back(i);
        }
    }
    
    if (config_.verbose_logging) {
        std::cout << "Unassigned observations: " << unassigned_observations.size() 
                  << ", Predictions: " << predictions.size() << std::endl;
    }
    
    // Solve assignment problem
    if (!unassigned_observations.empty() && !predictions.empty()) {
        auto assignment_result = assignment_algorithm_->solve(
            unassigned_observations, predictions, config_.assignment_constraints);
        
        if (assignment_result.success) {
            result.total_assignment_cost = assignment_result.total_cost;
            
            // Process assignments
            for (std::size_t i = 0; i < assignment_result.assignments.size(); ++i) {
                int assigned_prediction = assignment_result.assignments[i];
                
                if (assigned_prediction >= 0 && 
                    assigned_prediction < static_cast<int>(prediction_group_ids.size())) {
                    
                    // Update group with assigned observation
                    GroupId group_id = prediction_group_ids[assigned_prediction];
                    std::size_t obs_idx = unassigned_indices[i];
                    
                    group_tracker_->updateGroup(group_id, observations[obs_idx], dt);
                    result.updated_groups.push_back(group_id);
                    observation_assigned[obs_idx] = true;
                    
                    if (config_.verbose_logging) {
                        std::cout << "Assigned observation " << obs_idx << " -> group " 
                                  << group_id << " (cost: " << assignment_result.costs[i] << ")" << std::endl;
                    }
                } else {
                    // Unassigned observation
                    result.unassigned_objects.push_back(unassigned_indices[i]);
                }
            }
        } else {
            // Assignment failed, all remain unassigned
            result.unassigned_objects = unassigned_indices;
        }
    } else {
        // No assignment possible
        result.unassigned_objects = unassigned_indices;
    }
    
    // Create new groups for unassigned observations if enabled
    if (config_.create_new_groups && !result.unassigned_objects.empty()) {
        std::vector<FeatureVector> unassigned_features;
        for (auto obs_idx : result.unassigned_objects) {
            unassigned_features.push_back(observations[obs_idx]);
        }
        
        auto new_groups = createNewGroups(unassigned_features);
        result.new_groups = new_groups;
        result.unassigned_objects.clear(); // All assigned to new groups
        
        if (config_.verbose_logging) {
            std::cout << "Created " << new_groups.size() << " new groups" << std::endl;
        }
    }
    
    result.success = true;
    return result;
}

std::unordered_map<GroupId, FeaturePrediction> TrackingSession::getPredictions(double time) {
    std::unordered_map<GroupId, FeaturePrediction> predictions;
    
    double dt = time - current_time_;
    if (dt > config_.max_prediction_time) {
        if (config_.verbose_logging) {
            std::cout << "Prediction time " << dt << " exceeds maximum " 
                      << config_.max_prediction_time << std::endl;
        }
        return predictions; // Empty predictions
    }
    
    auto tracked_groups = group_tracker_->getTrackedGroups();
    for (auto group_id : tracked_groups) {
        auto prediction = group_tracker_->predictGroup(group_id, dt);
        if (prediction.valid) {
            predictions[group_id] = prediction;
        }
    }
    
    return predictions;
}

std::unordered_map<GroupId, FeatureVector> TrackingSession::getCurrentFeatures() const {
    std::unordered_map<GroupId, FeatureVector> current_features;
    
    auto tracked_groups = group_tracker_->getTrackedGroups();
    for (auto group_id : tracked_groups) {
        current_features[group_id] = group_tracker_->getCurrentFeatures(group_id);
    }
    
    return current_features;
}

bool TrackingSession::isGroupTracked(GroupId group_id) const {
    return group_tracker_->isGroupTracked(group_id);
}

void TrackingSession::removeGroup(GroupId group_id) {
    group_tracker_->removeGroup(group_id);
    
    if (config_.verbose_logging) {
        std::cout << "Removed group " << group_id << " from tracking" << std::endl;
    }
}

std::vector<GroupId> TrackingSession::getTrackedGroups() const {
    return group_tracker_->getTrackedGroups();
}

void TrackingSession::reset() {
    group_tracker_->reset();
    current_time_ = 0.0;
    next_new_group_id_ = 1000000;
    
    if (config_.verbose_logging) {
        std::cout << "Reset tracking session" << std::endl;
    }
}

void TrackingSession::setAssignmentAlgorithm(std::unique_ptr<AssignmentProblem> algorithm) {
    assignment_algorithm_ = std::move(algorithm);
}

std::vector<GroupId> TrackingSession::createNewGroups(std::vector<FeatureVector> const& unassigned_observations) {
    std::vector<GroupId> new_groups;
    
    for (auto const& features : unassigned_observations) {
        GroupId new_group_id = generateNewGroupId();
        initializeGroup(new_group_id, features, current_time_);
        new_groups.push_back(new_group_id);
    }
    
    return new_groups;
}

GroupId TrackingSession::generateNewGroupId() {
    return next_new_group_id_++;
}

// ========== DataTrackingBridge Implementation ==========

template<typename DataType>
DataTrackingBridge<DataType>::DataTrackingBridge(
    std::unique_ptr<FeatureExtractor<DataType>> feature_extractor,
    TrackingSessionConfig config)
    : feature_extractor_(std::move(feature_extractor)), tracking_session_(config) {}

template<typename DataType>
TrackingUpdateResult DataTrackingBridge<DataType>::processTimeFrame(
    std::vector<DataType> const& data_objects,
    double time_frame,
    std::unordered_map<std::size_t, GroupId> const& ground_truth_groups) {
    
    // Extract features from all data objects
    std::vector<FeatureVector> feature_vectors;
    feature_vectors.reserve(data_objects.size());
    
    for (auto const& data_obj : data_objects) {
        feature_vectors.push_back(feature_extractor_->extractFeatures(data_obj));
    }
    
    // Process with tracking session
    return tracking_session_.processObservations(feature_vectors, time_frame, ground_truth_groups);
}

// Explicit template instantiation for Line2D
template class DataTrackingBridge<Line2D>;

} // namespace StateEstimation
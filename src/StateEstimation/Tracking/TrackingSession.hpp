#ifndef STATE_ESTIMATION_TRACKING_SESSION_HPP
#define STATE_ESTIMATION_TRACKING_SESSION_HPP

#include "Features/FeatureVector.hpp"
#include "Assignment/AssignmentProblem.hpp"
#include "Tracking/MultiFeatureKalman.hpp"
#include <memory>
#include <functional>

namespace StateEstimation {

/**
 * @brief Configuration for a tracking session
 */
struct TrackingSessionConfig {
    MultiFeatureKalmanConfig kalman_config;
    AssignmentConstraints assignment_constraints;
    
    // Session parameters
    double max_prediction_time = 5.0;          // Maximum time to predict without updates
    double confidence_threshold = 0.1;         // Minimum confidence for predictions
    bool create_new_groups = false;            // Whether to create new groups for unassigned objects
    bool verbose_logging = false;              // Enable detailed logging
};

/**
 * @brief Result of a tracking update
 */
struct TrackingUpdateResult {
    std::vector<GroupId> updated_groups;       // Groups that received updates
    std::vector<std::size_t> unassigned_objects; // Indices of unassigned objects
    std::vector<GroupId> new_groups;           // Newly created groups (if enabled)
    double total_assignment_cost;              // Total cost of assignments
    bool success;                              // Whether update succeeded
    
    TrackingUpdateResult() : total_assignment_cost(0.0), success(false) {}
};

/**
 * @brief Manages a complete tracking session with multiple groups and features
 * 
 * This class orchestrates the complete tracking pipeline:
 * 1. Feature extraction from data objects
 * 2. Kalman filter prediction for existing groups
 * 3. Assignment of new observations to predicted groups
 * 4. Kalman filter updates with assigned observations
 * 5. Optional creation of new groups for unassigned observations
 */
class TrackingSession {
public:
    /**
     * @brief Construct with configuration
     */
    explicit TrackingSession(TrackingSessionConfig config = {});
    
    /**
     * @brief Initialize a group with ground truth observation
     * @param group_id Group identifier (from EntityGroupManager)
     * @param features Initial feature observation
     * @param time Initial time
     */
    void initializeGroup(GroupId group_id, FeatureVector const& features, double time = 0.0);
    
    /**
     * @brief Process a set of observations at a given time
     * @param observations Feature vectors for all observations at this time
     * @param time Current time
     * @param ground_truth_assignments Optional ground truth assignments (observation_index -> group_id)
     * @return Result of the tracking update
     */
    TrackingUpdateResult processObservations(
        std::vector<FeatureVector> const& observations,
        double time,
        std::unordered_map<std::size_t, GroupId> const& ground_truth_assignments = {}
    );
    
    /**
     * @brief Get predictions for all active groups
     * @param time Time for prediction
     * @return Map of group_id to prediction
     */
    std::unordered_map<GroupId, FeaturePrediction> getPredictions(double time);
    
    /**
     * @brief Get current features for all active groups
     */
    std::unordered_map<GroupId, FeatureVector> getCurrentFeatures() const;
    
    /**
     * @brief Check if a group is being tracked
     */
    [[nodiscard]] bool isGroupTracked(GroupId group_id) const;
    
    /**
     * @brief Remove a group from tracking
     */
    void removeGroup(GroupId group_id);
    
    /**
     * @brief Get all tracked group IDs
     */
    [[nodiscard]] std::vector<GroupId> getTrackedGroups() const;
    
    /**
     * @brief Reset the entire session
     */
    void reset();
    
    /**
     * @brief Set assignment algorithm
     */
    void setAssignmentAlgorithm(std::unique_ptr<AssignmentProblem> algorithm);
    
    /**
     * @brief Get current configuration
     */
    [[nodiscard]] TrackingSessionConfig const& getConfig() const { return config_; }

private:
    TrackingSessionConfig config_;
    std::unique_ptr<GroupTracker> group_tracker_;
    std::unique_ptr<AssignmentProblem> assignment_algorithm_;
    double current_time_;
    GroupId next_new_group_id_;
    
    /**
     * @brief Create new groups for unassigned observations
     */
    std::vector<GroupId> createNewGroups(std::vector<FeatureVector> const& unassigned_observations);
    
    /**
     * @brief Generate a new group ID for automatically created groups
     */
    GroupId generateNewGroupId();
};

/**
 * @brief Callback function type for tracking progress
 */
using TrackingProgressCallback = std::function<void(int percentage, std::string const& status)>;

/**
 * @brief Helper class to bridge between the new StateEstimation framework and existing LineData tracking
 */
template<typename DataType>
class DataTrackingBridge {
public:
    /**
     * @brief Construct with feature extractor and tracking session
     */
    DataTrackingBridge(std::unique_ptr<FeatureExtractor<DataType>> feature_extractor,
                       TrackingSessionConfig config = {});
    
    /**
     * @brief Process data objects at a specific time frame
     * @param data_objects Vector of data objects to track
     * @param time_frame Time frame identifier
     * @param ground_truth_groups Optional ground truth group assignments
     * @return Tracking update result
     */
    TrackingUpdateResult processTimeFrame(
        std::vector<DataType> const& data_objects,
        double time_frame,
        std::unordered_map<std::size_t, GroupId> const& ground_truth_groups = {}
    );
    
    /**
     * @brief Get the underlying tracking session
     */
    TrackingSession& getTrackingSession() { return tracking_session_; }
    
    /**
     * @brief Get the feature extractor
     */
    FeatureExtractor<DataType>& getFeatureExtractor() { return *feature_extractor_; }

private:
    std::unique_ptr<FeatureExtractor<DataType>> feature_extractor_;
    TrackingSession tracking_session_;
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_TRACKING_SESSION_HPP
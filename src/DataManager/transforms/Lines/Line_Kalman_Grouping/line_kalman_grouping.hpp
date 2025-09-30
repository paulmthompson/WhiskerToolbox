#ifndef LINE_KALMAN_GROUPING_HPP
#define LINE_KALMAN_GROUPING_HPP

#include "transforms/grouping_transforms.hpp"
#include "CoreGeometry/lines.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "StateEstimation/Kalman/kalman.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <vector>
#include <map>
#include <Eigen/Dense>

class LineData;

/**
 * @brief Parameters for the line Kalman grouping operation
 * 
 * This operation uses Kalman filtering and smoothing to track line centroids
 * across time frames and assign ungrouped lines to existing groups using
 * the Hungarian algorithm with Mahalanobis distance.
 */
struct LineKalmanGroupingParameters : public GroupingTransformParametersBase {
    explicit LineKalmanGroupingParameters(EntityGroupManager* group_manager)
        : GroupingTransformParametersBase(group_manager) {}

    // === Kalman Filter Parameters ===
    double dt = 1.0;                                    // Time step between frames
    double process_noise_position = 10.0;               // Process noise for position
    double process_noise_velocity = 1.0;                // Process noise for velocity
    double measurement_noise = 5.0;                     // Measurement noise for observations
    double initial_position_uncertainty = 50.0;         // Initial uncertainty in position
    double initial_velocity_uncertainty = 10.0;         // Initial uncertainty in velocity
    
    // === Assignment Parameters ===
    double max_assignment_distance = 100.0;             // Maximum Mahalanobis distance for assignment
    double min_kalman_confidence = 0.1;                 // Minimum confidence threshold for predictions
    
    // === Algorithm Control ===
    bool estimate_noise_empirically = true;             // Estimate noise from existing grouped data
    bool run_backward_smoothing = true;                 // Run RTS smoothing between anchor frames
    bool create_new_group_for_outliers = false;         // Create new groups for unassigned lines
    std::string new_group_name = "Kalman Tracked Lines"; // Name for new groups if created
    
    // === Debugging/Validation ===
    bool verbose_output = false;                         // Enable detailed logging
};

/**
 * @brief Structure to hold the state of a group being tracked
 */
struct GroupTrackingState {
    GroupId group_id;
    KalmanFilter kalman_filter;
    std::vector<TimeFrameIndex> anchor_frames;           // Frames with manual labels
    std::vector<Eigen::Vector2d> anchor_centroids;      // Centroids at anchor frames
    TimeFrameIndex last_update_frame = TimeFrameIndex(0);
    bool is_active = false;
    double confidence = 1.0;
};

/**
 * @brief Calculate the centroid of a line
 * 
 * @param line The line to calculate centroid for
 * @return 2D centroid position
 */
Eigen::Vector2d calculateLineCentroid(Line2D const& line);

/**
 * @brief Estimate process and measurement noise from existing grouped data
 * 
 * @param line_data The LineData containing all lines
 * @param group_manager The EntityGroupManager containing existing groups
 * @param params Parameters to update with estimated noise values
 */
void estimateNoiseFromGroupedData(LineData const* line_data,
                                  EntityGroupManager* group_manager,
                                  LineKalmanGroupingParameters* params);

/**
 * @brief Initialize Kalman filter for position and velocity tracking
 * 
 * @param params Parameters containing noise settings
 * @return Configured KalmanFilter
 */
KalmanFilter createKalmanFilter(LineKalmanGroupingParameters const* params);

/**
 * @brief Calculate Mahalanobis distance between observation and prediction
 * 
 * @param observation Observed centroid position
 * @param prediction Kalman filter predicted position
 * @param covariance Covariance matrix from Kalman filter
 * @return Mahalanobis distance
 */
double calculateMahalanobisDistance(Eigen::Vector2d const& observation,
                                    Eigen::Vector2d const& prediction,
                                    Eigen::Matrix2d const& covariance);

/**
 * @brief Solve assignment problem using Hungarian algorithm
 * 
 * @param ungrouped_centroids Centroids of ungrouped lines
 * @param predicted_centroids Predicted centroids from Kalman filters
 * @param covariances Covariance matrices for each prediction
 * @param max_distance Maximum allowed assignment distance
 * @return Vector of assignments (ungrouped_index -> prediction_index, -1 for unassigned)
 */
std::vector<int> solveAssignmentProblem(std::vector<Eigen::Vector2d> const& ungrouped_centroids,
                                        std::vector<Eigen::Vector2d> const& predicted_centroids,
                                        std::vector<Eigen::Matrix2d> const& covariances,
                                        double max_distance);

/**
 * @brief Run RTS (Rauch-Tung-Striebel) smoothing between anchor frames
 * 
 * @param tracking_states Map of group tracking states
 * @param start_frame Start frame for smoothing interval
 * @param end_frame End frame for smoothing interval
 * @param params Algorithm parameters
 */
void runRTSSmoothing(std::map<GroupId, GroupTrackingState>& tracking_states,
                     TimeFrameIndex start_frame,
                     TimeFrameIndex end_frame,
                     LineKalmanGroupingParameters const* params);

/**
 * @brief Main function: Group lines using Kalman filtering and assignment
 * 
 * This function processes all time frames, using existing grouped lines as
 * anchor points and tracking centroids with Kalman filters. Ungrouped lines
 * are assigned to groups using the Hungarian algorithm with Mahalanobis distance.
 * 
 * @param line_data The LineData to process
 * @param params Parameters including thresholds and Kalman settings
 * @return The same LineData shared_ptr (operation is performed in-place on groups)
 */
std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params);

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params,
                                             ProgressCallback progressCallback);

/**
 * @brief Transform operation for Kalman-based line grouping
 */
class LineKalmanGroupingOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    
    [[nodiscard]] bool canApply(DataTypeVariant const& dataVariant) const override;
    
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    
    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                           TransformParametersBase const* transformParameters,
                           ProgressCallback progressCallback) override;
                           
    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                           TransformParametersBase const* transformParameters) override;
};

#endif // LINE_KALMAN_GROUPING_HPP
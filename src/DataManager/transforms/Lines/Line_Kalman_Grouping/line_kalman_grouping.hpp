#ifndef LINE_KALMAN_GROUPING_HPP
#define LINE_KALMAN_GROUPING_HPP

#include "transforms/grouping_transforms.hpp"
#include "CoreGeometry/lines.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "StateEstimation/Tracker.hpp"
#include "StateEstimation/Kalman/KalmanFilter.hpp"
#include "StateEstimation/Features/IFeatureExtractor.hpp"
#include "StateEstimation/Assignment/HungarianAssigner.hpp"
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
 * This operation uses Kalman filtering and smoothing to track multiple line features
 * (centroid + base point) across time frames and assign ungrouped lines to existing 
 * groups using the Hungarian algorithm with Mahalanobis distance.
 * 
 * Features tracked:
 * - Line centroid (center of mass of all points)
 * - Line base point (first point in the line)
 * 
 * These features are concatenated into a 4D measurement vector for tracking.
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
    
    // === Debugging/Validation ===
    bool verbose_output = false;                         // Enable detailed logging
};

/**
 * @brief Main function: Group lines using Kalman filtering and assignment
 * 
 * This function processes all time frames, using existing grouped lines as
 * anchor points and tracking multiple line features (centroid + base point) with 
 * Kalman filters. Ungrouped lines are assigned to groups using the Hungarian 
 * algorithm with Mahalanobis distance.
 * 
 * The tracker operates directly on Line2D objects with on-demand feature extraction,
 * avoiding unnecessary memory overhead. Features are computed only when needed for
 * prediction, update, or assignment operations.
 * 
 * @param line_data The LineData to process
 * @param params Parameters including thresholds and Kalman settings
 * @return The same LineData shared_ptr (operation is performed in-place on groups)
 */
std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params);

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params,
                                             ProgressCallback const& progressCallback);

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
                           TransformParametersBase const* transformParameters) override;
                           
    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                           TransformParametersBase const* transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif // LINE_KALMAN_GROUPING_HPP
#ifndef LINE_KALMAN_GROUPING_HPP
#define LINE_KALMAN_GROUPING_HPP

#include "CoreGeometry/lines.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/grouping_transforms.hpp"

#include <Eigen/Dense>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

class LineData;

/**
 * @brief Parameters for the line Kalman grouping operation
 * 
 * This operation uses Kalman filtering and smoothing to track multiple line features
 * across time frames and assign ungrouped lines to existing groups using the 
 * Hungarian algorithm with Mahalanobis distance.
 * 
 * Features tracked:
 * - Line centroid (center of mass of all points) - KINEMATIC_2D
 * - Line base point (first point in the line) - KINEMATIC_2D
 * - Line length (total arc length) - STATIC
 * 
 * The metadata-driven system automatically handles different temporal behaviors:
 * - Kinematic features: Track position + velocity (2D measurement → 4D state)
 * - Static features: Track value only, no velocity (1D measurement → 1D state)
 * 
 * Total measurement space: 5D (2D + 2D + 1D)
 * Total state space: 9D (4D + 4D + 1D)
 */
struct LineKalmanGroupingParameters : public GroupingTransformParametersBase {
    explicit LineKalmanGroupingParameters(EntityGroupManager * group_manager = nullptr)
        : GroupingTransformParametersBase(group_manager) {}

    // === Kalman Filter Parameters ===
    double dt = 1.0;// Time step between frames

    // Process noise for kinematic features (centroid, base point)
    double process_noise_position = 10.0;// Process noise for position (pixels)
    double process_noise_velocity = 1.0; // Process noise for velocity (pixels/frame)

    // Process noise for static features (length)
    double static_feature_process_noise_scale = 0.01;// Multiplier for static features (0.01 = 100x less noise)

    // Automatic noise estimation from ground truth data
    bool auto_estimate_static_noise = false;// Estimate static feature noise from ground truth
    double static_noise_percentile = 0.1;   // Use this fraction of observed variance (e.g., 0.1 = 10%)

    // Measurement noise per feature type
    double measurement_noise_position = 5.0;     // Measurement noise for x,y coordinates (pixels)
    double measurement_noise_length = 10.0;      // Measurement noise for length (pixels)
    bool auto_estimate_measurement_noise = false;// Estimate measurement noise from ground truth residuals

    // Initial uncertainties
    double initial_position_uncertainty = 50.0;// Initial uncertainty in position
    double initial_velocity_uncertainty = 10.0;// Initial uncertainty in velocity

    // === Linkage and Optimization Parameters ===
    // Threshold (in Mahalanobis distance units) for greedy frame-to-frame linkage into meta-nodes
    double cheap_assignment_threshold = 5.0;
    // Scale factor for converting costs to integers for OR-Tools
    double cost_scale_factor = 100.0;

    // === Debugging/Validation ===
    bool verbose_output = false;// Enable detailed logging

    // === Output Control ===
    bool write_to_putative_groups = true;           // If true, write results to new putative groups per anchor group
    std::string putative_group_prefix = "Putative:";// Prefix for new putative groups
    
    // === Cross-Feature Covariance ===
    // Enable modeling of correlations between features (e.g., position-length coupling due to camera clipping)
    // When enabled, correlations are automatically estimated from ground truth data.
    // The Kalman filter then naturally propagates and refines these correlations over time.
    bool enable_cross_feature_covariance = false;
    
    // Minimum absolute correlation to include (filter out weak correlations)
    // Correlations below this threshold are treated as zero (independent features)
    double min_correlation_threshold = 0.1;
};

/**
 * @brief Main function: Group lines using Kalman filtering and assignment
 * 
 * This function processes all time frames, using existing grouped lines as
 * anchor points and tracking multiple line features (centroid + base point + length) 
 * with Kalman filters. Ungrouped lines are assigned to groups using the Hungarian 
 * algorithm with Mahalanobis distance.
 * 
 * The tracker operates directly on Line2D objects with on-demand feature extraction,
 * avoiding unnecessary memory overhead. Features are computed only when needed for
 * prediction, update, or assignment operations.
 * 
 * The system uses a metadata-driven approach to handle features with different
 * temporal behaviors: kinematic features (with velocity) and static features
 * (time-invariant) are combined seamlessly in the same tracking system.
 * 
 * @param line_data The LineData to process
 * @param params Parameters including thresholds and Kalman settings
 * @return The same LineData shared_ptr (operation is performed in-place on groups)
 */
std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params);

std::shared_ptr<LineData> lineKalmanGrouping(std::shared_ptr<LineData> line_data,
                                             LineKalmanGroupingParameters const * params,
                                             ProgressCallback const & progressCallback);

/**
 * @brief Transform operation for Kalman-based line grouping
 */
class LineKalmanGroupingOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the Kalman grouping operation.
     * @return A unique_ptr to the default parameters with null group manager.
     * @note The EntityGroupManager must be set via setGroupManager() before execution.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif// LINE_KALMAN_GROUPING_HPP
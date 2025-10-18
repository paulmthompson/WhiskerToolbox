#ifndef LINE_OUTLIER_DETECTION_HPP
#define LINE_OUTLIER_DETECTION_HPP

#include "Entity/EntityGroupManager.hpp"
#include "StateEstimation/Common.hpp"
#include "transforms/grouping_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <vector>

class LineData;

/**
 * @brief Parameters for the line outlier detection operation
 * 
 * This operation uses Kalman filtering and smoothing to detect outliers in grouped line data.
 * It processes each existing group independently, performing forward-backward smoothing and
 * identifying entities that deviate significantly from the predicted trajectory.
 * 
 * Features used for outlier detection:
 * - Line centroid (center of mass of all points)
 * - Line length (total arc length)
 * 
 * The algorithm uses Median Absolute Deviation (MAD) to identify outliers:
 * - Entities with cost > median + (mad_threshold * MAD) are marked as outliers
 */
struct LineOutlierDetectionParameters : public GroupingTransformParametersBase {
    explicit LineOutlierDetectionParameters(EntityGroupManager * group_manager = nullptr)
        : GroupingTransformParametersBase(group_manager) {}

    // === Kalman Filter Parameters ===
    double dt = 1.0;                        // Time step between frames
    double process_noise_position = 10.0;   // Process noise for position (pixels)
    double process_noise_velocity = 1.0;    // Process noise for velocity (pixels/frame)
    double process_noise_length = 0.1;      // Process noise for length (pixels)
    double measurement_noise_position = 5.0; // Measurement noise for x,y coordinates (pixels)
    double measurement_noise_length = 10.0;  // Measurement noise for length (pixels)
    
    // Initial uncertainties
    double initial_position_uncertainty = 50.0; // Initial uncertainty in position
    double initial_velocity_uncertainty = 10.0; // Initial uncertainty in velocity
    double initial_length_uncertainty = 20.0;   // Initial uncertainty in length

    // === Outlier Detection Parameters ===
    double mad_threshold = 5.0; // Number of MAD units above median to classify as outlier
    
    // === Group Selection ===
    // If empty, process all groups. Otherwise, only process specified groups
    std::vector<GroupId> groups_to_process;
    
    // === Output Control ===
    std::string outlier_group_name = "Outliers"; // Name for the outlier group
    bool verbose_output = false;                  // Enable detailed logging
};

/**
 * @brief Main function: Detect outliers in grouped lines using Kalman filtering
 * 
 * This function processes existing groups in the LineData, using Kalman filtering
 * and smoothing to establish expected trajectories. Entities that deviate significantly
 * from their group's predicted behavior are identified as outliers and moved to a
 * dedicated outlier group.
 * 
 * The algorithm:
 * 1. For each group with entities in the LineData
 * 2. Extract features (centroid + length) for each entity in the group
 * 3. Perform forward Kalman filtering
 * 4. Perform backward smoothing using Rauch-Tung-Striebel smoother
 * 5. Calculate Mahalanobis distance between smoothed prediction and actual measurement
 * 6. Identify outliers using MAD (Median Absolute Deviation) criterion
 * 7. Add outliers to a new "Outliers" group
 * 
 * @param line_data The LineData to process
 * @param params Parameters including thresholds and Kalman settings
 * @return The same LineData shared_ptr (operation is performed in-place on groups)
 */
std::shared_ptr<LineData> lineOutlierDetection(std::shared_ptr<LineData> line_data,
                                               LineOutlierDetectionParameters const * params);

std::shared_ptr<LineData> lineOutlierDetection(std::shared_ptr<LineData> line_data,
                                               LineOutlierDetectionParameters const * params,
                                               StateEstimation::ProgressCallback const & progressCallback);

/**
 * @brief Transform operation for Kalman-based line outlier detection
 */
class LineOutlierDetectionOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the outlier detection operation.
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

#endif // LINE_OUTLIER_DETECTION_HPP

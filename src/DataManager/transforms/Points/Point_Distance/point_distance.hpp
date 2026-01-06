#ifndef WHISKERTOOLBOX_POINT_DISTANCE_HPP
#define WHISKERTOOLBOX_POINT_DISTANCE_HPP

#include "transforms/data_transforms.hpp"

#include <memory>       // std::shared_ptr
#include <string>       // std::string
#include <typeindex>    // std::type_index

class AnalogTimeSeries;
class PointData;

/**
 * @brief Reference point type for distance calculation
 */
enum class PointDistanceReferenceType {
    GlobalAverage,      // Average of all X and Y values across all time
    RollingAverage,     // Rolling average of X and Y values over a window
    SetPoint,           // User-specified fixed point
    OtherPointData      // Another PointData object (e.g., compare jaw to tongue)
};

/**
 * @brief Calculate the euclidean distance of points from a reference
 *
 * @param point_data The point data to calculate distances from
 * @param reference_type Type of reference point to use
 * @param window_size Window size for rolling average (only used for RollingAverage)
 * @param reference_x X coordinate for set point (only used for SetPoint)
 * @param reference_y Y coordinate for set point (only used for SetPoint)
 * @param reference_point_data Reference point data (only used for OtherPointData)
 * @return A new AnalogTimeSeries containing distance values at each timestamp
 */
std::shared_ptr<AnalogTimeSeries> pointDistance(
    PointData const * point_data,
    PointDistanceReferenceType reference_type = PointDistanceReferenceType::GlobalAverage,
    int window_size = 1000,
    float reference_x = 0.0f,
    float reference_y = 0.0f,
    PointData const * reference_point_data = nullptr);

std::shared_ptr<AnalogTimeSeries> pointDistance(
    PointData const * point_data,
    PointDistanceReferenceType reference_type,
    int window_size,
    float reference_x,
    float reference_y,
    PointData const * reference_point_data,
    ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

struct PointDistanceParameters : public TransformParametersBase {
    PointDistanceReferenceType reference_type = PointDistanceReferenceType::GlobalAverage;
    int window_size = 1000;
    float reference_x = 0.0f;
    float reference_y = 0.0f;
    std::shared_ptr<PointData> reference_point_data;
};

class PointDistanceOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Gets the default parameters for the point distance operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null PointData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Executes the point distance calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the PointData object.
     * @return DataTypeVariant containing a std::shared_ptr<AnalogTimeSeries> on success,
     * or an empty on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;
};

#endif//WHISKERTOOLBOX_POINT_DISTANCE_HPP


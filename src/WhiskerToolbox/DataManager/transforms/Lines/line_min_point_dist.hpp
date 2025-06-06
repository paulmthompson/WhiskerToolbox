#ifndef LINE_MIN_POINT_DIST_HPP
#define LINE_MIN_POINT_DIST_HPP

#include "transforms/data_transforms.hpp"

#include "Points/points.hpp"
#include "ImageSize/ImageSize.hpp"

#include <memory>       // std::shared_ptr
#include <string>       // std::string
#include <typeindex>    // std::type_index

class AnalogTimeSeries;
class LineData;
class PointData;

float point_to_line_segment_distance2(
        Point2D<float> const & point,
        Point2D<float> const & line_start,
        Point2D<float> const & line_end);

Point2D<float> scale_point(Point2D<float> const & point, 
                           ImageSize const & from_size, 
                           ImageSize const & to_size);


///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<AnalogTimeSeries> line_min_point_dist(LineData const * line_data,
                                                      PointData const * point_data);

/**
 * @brief Calculate the minimum distance from points to a line at each timestamp
 *
 * @param line_data The line data to measure distances from
 * @param point_data The point data to measure distances to
 * @return A new AnalogTimeSeries containing minimum distances at each timestamp
 */
std::shared_ptr<AnalogTimeSeries> line_min_point_dist(LineData const * line_data,
                                                      PointData const * point_data,
                                                      ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

struct LineMinPointDistParameters : public TransformParametersBase {
    std::shared_ptr<PointData> point_data;  // Pointer to the PointData
};

class LineMinPointDistOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null LineData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the line minimum point distance operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the line minimum point distance calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the LineData object.
     * @param transformParameters Parameters for the calculation, including the point data pointer.
     * @return DataTypeVariant containing a std::shared_ptr<AnalogTimeSeries> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;

    // Overload with progress
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif//LINE_MIN_POINT_DIST_HPP

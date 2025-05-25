#ifndef LINE_ANGLE_HPP
#define LINE_ANGLE_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index
#include <vector>   // std::vector

class AnalogTimeSeries;
class LineData;

enum class AngleCalculationMethod {
    DirectPoints,// Original method - directly calculate angle between two points
    PolynomialFit// Fit polynomials to the line and calculate angle from derivatives
};

struct LineAngleParameters : public TransformParametersBase {
    float position = 0.2f;                                               // Default 20% along the line
    AngleCalculationMethod method = AngleCalculationMethod::DirectPoints;// Default method
    int polynomial_order = 3;                                            // Default polynomial order

    // Reference vector parameters (what direction is 0 degrees)
    // Default is (1,0) which corresponds to the positive x-axis
    float reference_x = 1.0f;
    float reference_y = 0.0f;
};

class LineAngleOperation final : public TransformOperation {
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
     * @brief Gets the default parameters for the line angle operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the line angle calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the LineData object.
     * @param transformParameters Parameters for the angle calculation.
     * @return DataTypeVariant containing a std::shared_ptr<AnalogTimeSeries> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;
};


/**
 * @brief Calculate the angle at a specified position along a line at each timestamp
 *
 * @param line_data The line data to calculate angles from
 * @param params Parameters including the position along the line (0.0-1.0)
 * @return A new AnalogTimeSeries containing angle values at each timestamp
 */
std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data,
                                             LineAngleParameters const * params);

std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data,
                                             LineAngleParameters const * params,
                                             ProgressCallback progressCallback);

#endif//LINE_ANGLE_HPP

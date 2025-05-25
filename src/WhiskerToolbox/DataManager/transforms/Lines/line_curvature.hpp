#ifndef LINE_CURVATURE_HPP
#define LINE_CURVATURE_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr, std::unique_ptr
#include <optional> // std::optional
#include <string>   // std::string
#include <typeindex>// std::type_index


enum class CurvatureCalculationMethod {
    PolynomialFit// Only method for now
};

struct LineCurvatureParameters : public TransformParametersBase {
    float position = 0.5f;// Fractional distance along the line (0.0 to 1.0)
    CurvatureCalculationMethod method = CurvatureCalculationMethod::PolynomialFit;
    int polynomial_order = 3;              // Default polynomial order for fitting
    float fitting_window_percentage = 0.1f;// Default fitting window percentage
};

class LineCurvatureOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;
    // Overload with progress, though for curvature it might be quick per line
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

std::shared_ptr<AnalogTimeSeries> line_curvature(LineData const * line_data,
                                                 LineCurvatureParameters const * params);

std::shared_ptr<AnalogTimeSeries> line_curvature(LineData const * line_data,
                                                 LineCurvatureParameters const * params,
                                                 ProgressCallback progressCallback);

std::optional<float> calculate_polynomial_curvature(
        Line2D const & line,
        float t_position,
        int polynomial_order,
        float fitting_window_percentage);

#endif// LINE_CURVATURE_HPP

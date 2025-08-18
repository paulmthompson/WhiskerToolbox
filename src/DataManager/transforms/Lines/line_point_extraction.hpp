#ifndef LINE_POINT_EXTRACTION_HPP
#define LINE_POINT_EXTRACTION_HPP

#include "transforms/data_transforms.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <memory>   // std::shared_ptr
#include <optional> // std::optional
#include <string>   // std::string
#include <typeindex>// std::type_index


class LineData;
class PointData;

enum class PointExtractionMethod {
    Direct,          // Direct point selection based on indices
    Parametric       // Use parametric polynomial interpolation
};

struct LinePointExtractionParameters : public TransformParametersBase {
    float position = 0.5f;          // Position as fraction (0.0 to 1.0)
    
    PointExtractionMethod method = PointExtractionMethod::Parametric;
    
    // For parametric method
    int polynomial_order = 3;
    
    // For direct method
    bool use_interpolation = true;   // Interpolate between points vs. use nearest point
};

///////////////////////////////////////////////////////////////////////////////

std::optional<Point2D<float>> extract_direct_point(Line2D const & line,
                                                   float position,
                                                   bool use_interpolation);

std::optional<Point2D<float>> extract_parametric_point(Line2D const & line,
                                                       float position,
                                                       int polynomial_order);

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Extract a point from LineData at specified position
 * @param line_data The LineData to extract from
 * @param params The extraction parameters
 * @return A new PointData containing the extracted points
 */
std::shared_ptr<PointData> extract_line_point(
        LineData const * line_data,
        LinePointExtractionParameters const & params);

/**
 * @brief Extract a point from LineData with progress reporting
 * @param line_data The LineData to extract from
 * @param params The extraction parameters
 * @param progressCallback Progress reporting callback
 * @return A new PointData containing the extracted points
 */
std::shared_ptr<PointData> extract_line_point(
        LineData const * line_data,
        LinePointExtractionParameters const & params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class LinePointExtractionOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;
                           
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif // LINE_POINT_EXTRACTION_HPP 
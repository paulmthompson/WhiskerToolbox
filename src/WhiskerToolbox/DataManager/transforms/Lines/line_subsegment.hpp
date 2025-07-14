#ifndef LINE_SUBSEGMENT_HPP
#define LINE_SUBSEGMENT_HPP

#include "transforms/data_transforms.hpp"

#include "CoreGeometry/points.hpp"
#include "Lines/lines.hpp"


#include <memory>   // std::shared_ptr
#include <optional> // std::optional
#include <string>   // std::string
#include <typeindex>// std::type_index
#include <vector>   // std::vector

class LineData;

enum class SubsegmentExtractionMethod {
    Direct,          // Direct point extraction based on indices
    Parametric       // Use parametric polynomial interpolation
};

struct LineSubsegmentParameters : public TransformParametersBase {
    float start_position = 0.3f;    // Start position as fraction (0.0 to 1.0)
    float end_position = 0.7f;      // End position as fraction (0.0 to 1.0)
    
    SubsegmentExtractionMethod method = SubsegmentExtractionMethod::Parametric;
    
    // For parametric method
    int polynomial_order = 3;
    int output_points = 50;         // Number of points in output subsegment
    
    // For direct method
    bool preserve_original_spacing = true;  // Keep original point spacing vs. resample
};

///////////////////////////////////////////////////////////////////////////////

std::vector<Point2D<float>> extract_direct_subsegment(Line2D const & line,
                                                      float start_pos,
                                                      float end_pos,
                                                      bool preserve_spacing);

std::vector<Point2D<float>> extract_parametric_subsegment(Line2D const & line,
                                                          float start_pos,
                                                          float end_pos,
                                                          int polynomial_order,
                                                          int output_points);

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Extract a subsegment from LineData between specified positions
 * @param line_data The LineData to extract from
 * @param params The extraction parameters
 * @return A new LineData containing the extracted subsegments
 */
std::shared_ptr<LineData> extract_line_subsegment(
        LineData const * line_data,
        LineSubsegmentParameters const & params);

/**
 * @brief Extract a subsegment from LineData with progress reporting
 * @param line_data The LineData to extract from
 * @param params The extraction parameters
 * @param progressCallback Progress reporting callback
 * @return A new LineData containing the extracted subsegments
 */
std::shared_ptr<LineData> extract_line_subsegment(
        LineData const * line_data,
        LineSubsegmentParameters const & params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class LineSubsegmentOperation final : public TransformOperation {
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

#endif // LINE_SUBSEGMENT_HPP 
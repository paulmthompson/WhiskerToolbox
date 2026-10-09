#ifndef LINE_EXTEND_HPP
#define LINE_EXTEND_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

class LineData;

struct LineExtendParameters : public TransformParametersBase {
    std::shared_ptr<LineData> reference_line_data;
    int reference_frame = 0;
    ExtendEndpoint extend_end = ExtendEndpoint::Distal;
    float tangent_distance_pixels = 20.0f;
};

/**
 * @brief Check that input and reference lines share a defined, matching image size.
 */
[[nodiscard]] bool validateLineExtendImageSizes(
        LineData const * input_line_data,
        LineData const * reference_line_data);

/**
 * @brief Extend line data to a reference line
 */
std::shared_ptr<LineData> extend_lines(
        LineData const * line_data,
        LineExtendParameters const * params);

/**
 * @brief Extend line data to a reference line with progress reporting
 */
std::shared_ptr<LineData> extend_lines(
        LineData const * line_data,
        LineExtendParameters const * params,
        ProgressCallback const & progressCallback);

class LineExtendOperation final : public TransformOperation {
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

#endif// LINE_EXTEND_HPP

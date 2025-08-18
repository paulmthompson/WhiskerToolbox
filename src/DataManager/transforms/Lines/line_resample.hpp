#ifndef LINE_RESAMPLE_OPERATION_HPP
#define LINE_RESAMPLE_OPERATION_HPP

#include "transforms/data_transforms.hpp"

#include <memory>
#include <string>
#include <typeindex>

enum class LineSimplificationAlgorithm {
    FixedSpacing,
    DouglasPeucker
};

struct LineResampleParameters : public TransformParametersBase {
    LineSimplificationAlgorithm algorithm = LineSimplificationAlgorithm::FixedSpacing;
    float target_spacing = 5.0f;// Default desired spacing in pixels (for FixedSpacing)
    float epsilon = 2.0f;       // Default epsilon for Douglas-Peucker algorithm
    // Min/Max values will be handled by the UI widget
};

///////////////////////////////////////////////////////////////////////////////

class LineResampleOperation final : public TransformOperation {
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

#endif// LINE_RESAMPLE_OPERATION_HPP

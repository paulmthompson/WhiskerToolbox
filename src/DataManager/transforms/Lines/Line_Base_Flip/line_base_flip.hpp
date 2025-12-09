#ifndef LINE_BASE_FLIP_HPP
#define LINE_BASE_FLIP_HPP

#include "transforms/data_transforms.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"

#include <memory>
#include <string>
#include <typeindex>

class LineData;

/**
 * @brief Parameters for Line Base Flip transform
 */
class LineBaseFlipParameters : public TransformParametersBase {
public:
    Point2D<float> reference_point{0.0f, 0.0f};  // Manually placed reference point

    LineBaseFlipParameters() = default;
    LineBaseFlipParameters(Point2D<float> ref_point) : reference_point(ref_point) {}
};

/**
 * @brief Transform that flips the base of lines based on distance to a reference point
 *
 * This transform allows users to manually place a reference point in the media viewer.
 * For each line, it compares the distance from both endpoints to the reference point.
 * If the current base (first point) is farther from the reference than the end point,
 * the line is flipped so that the closer endpoint becomes the new base.
 */
class LineBaseFlipTransform : public TransformOperation {
public:
    LineBaseFlipTransform() = default;

    [[nodiscard]] std::string getName() const override {
        return "Line Base Flip";
    }

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override {
        return std::type_index(typeid(std::shared_ptr<LineData>));
    }

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override {
        return std::make_unique<LineBaseFlipParameters>();
    }

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;

private:
    /**
     * @brief Determine if a line should be flipped based on reference point
     * @param line The line to evaluate
     * @param reference_point The manually placed reference point
     * @return true if the line should be flipped, false otherwise
     */
    bool shouldFlipLine(Line2D const & line, Point2D<float> const & reference_point);

    /**
     * @brief Flip a line by reversing the order of its points
     * @param line The line to flip
     * @return The flipped line
     */
    Line2D flipLine(Line2D const & line);
};

#endif // LINE_BASE_FLIP_HPP

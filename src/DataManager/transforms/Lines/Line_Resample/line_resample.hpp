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

class LineData;

/**
 * @brief Resamples lines in a LineData object based on the specified algorithm.
 *
 * @param line_data The LineData to process.
 * @param params Parameters for resampling, including algorithm, target spacing, and epsilon.
 * @return A new LineData containing the resampled lines.
 */
std::shared_ptr<LineData> line_resample(
        LineData const * line_data,
        LineResampleParameters const & params);

/**
 * @brief Resamples lines in a LineData object based on the specified algorithm, with progress reporting.
 *
 * @param line_data The LineData to process.
 * @param params Parameters for resampling, including algorithm, target spacing, and epsilon.
 * @param progressCallback A callback function to report progress (0-100).
 * @return A new LineData containing the resampled lines.
 */
std::shared_ptr<LineData> line_resample(
        LineData const * line_data,
        LineResampleParameters const & params,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class LineResampleOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;
    /**
     * @brief Executes the line resampling operation with progress reporting.
     *
     * This method retrieves a LineData from the input dataVariant,
     * applies the line resampling logic using the provided parameters,
     * and reports progress via the progressCallback.
     *
     * @param dataVariant A variant holding a std::shared_ptr<LineData>.
     *                    The operation will fail if the variant holds a different type,
     *                    or if the shared_ptr is null.
     * @param transformParameters A pointer to TransformParametersBase, which is expected
     *                            to be dynamically castable to LineResampleParameters. If null or
     *                            of an incorrect type, default LineResampleParameters will be used.
     * @param progressCallback An optional function to report progress (0-100).
     *                         If nullptr, progress is not reported.
     * @return A DataTypeVariant containing a std::shared_ptr<LineData> with the
     *         resampled lines on success. Returns an empty DataTypeVariant on failure
     *         (e.g., type mismatch, null input data, or if the underlying
     *         line_resample function fails).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif// LINE_RESAMPLE_OPERATION_HPP

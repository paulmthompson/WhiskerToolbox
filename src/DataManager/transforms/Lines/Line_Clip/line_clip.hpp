#ifndef LINE_CLIP_HPP
#define LINE_CLIP_HPP

#include "transforms/data_transforms.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/line_geometry.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <optional>

class LineData;



struct LineClipParameters : public TransformParametersBase {
    std::shared_ptr<LineData> reference_line_data;  // The line data to use for clipping
    int reference_frame = 0;                        // Which frame from reference line to use
    ClipSide clip_side = ClipSide::KeepBase;       // Which side of the intersection to keep
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Clip line data using a reference line
 * @param line_data The LineData to clip
 * @param params The clipping parameters
 * @return A new LineData containing the clipped lines
 */
std::shared_ptr<LineData> clip_lines(
    LineData const * line_data,
    LineClipParameters const * params);

/**
 * @brief Clip line data using a reference line with progress reporting
 * @param line_data The LineData to clip
 * @param params The clipping parameters
 * @param progressCallback Progress reporting callback
 * @return A new LineData containing the clipped lines
 */
std::shared_ptr<LineData> clip_lines(
    LineData const * line_data,
    LineClipParameters const * params,
    ProgressCallback progressCallback);

class LineClipOperation final : public TransformOperation {
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

#endif // LINE_CLIP_HPP 

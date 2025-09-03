#ifndef LINE_CLIP_HPP
#define LINE_CLIP_HPP

#include "transforms/data_transforms.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <optional>

class LineData;

enum class ClipSide {
    KeepBase,    // Keep the portion from line start to intersection
    KeepDistal   // Keep the portion from intersection to line end
};

struct LineClipParameters : public TransformParametersBase {
    std::shared_ptr<LineData> reference_line_data;  // The line data to use for clipping
    int reference_frame = 0;                        // Which frame from reference line to use
    ClipSide clip_side = ClipSide::KeepBase;       // Which side of the intersection to keep
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Find intersection point between two line segments
 * @param p1 Start point of first line segment
 * @param p2 End point of first line segment  
 * @param p3 Start point of second line segment
 * @param p4 End point of second line segment
 * @return Intersection point if segments intersect, nullopt otherwise
 */
std::optional<Point2D<float>> line_segment_intersection(
    Point2D<float> const & p1, Point2D<float> const & p2,
    Point2D<float> const & p3, Point2D<float> const & p4);

/**
 * @brief Find the first intersection between a line and a reference line
 * @param line The line to check for intersections
 * @param reference_line The reference line to intersect with
 * @return Intersection point and segment index if found, nullopt otherwise
 */
std::optional<std::pair<Point2D<float>, size_t>> find_line_intersection(
    Line2D const & line, Line2D const & reference_line);

/**
 * @brief Clip a line at the intersection with a reference line
 * @param line The line to clip
 * @param reference_line The reference line to clip against
 * @param clip_side Which side of the intersection to keep
 * @return Clipped line, or original line if no intersection found
 */
Line2D clip_line_at_intersection(
    Line2D const & line, 
    Line2D const & reference_line, 
    ClipSide clip_side);

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

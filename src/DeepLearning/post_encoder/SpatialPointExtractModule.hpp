/**
 * @file SpatialPointExtractModule.hpp
 * @brief Point-based spatial feature extraction post-encoder module.
 */

#ifndef NEURALYZER_SPATIAL_POINT_EXTRACT_MODULE_HPP
#define NEURALYZER_SPATIAL_POINT_EXTRACT_MODULE_HPP

#include "PostEncoderModule.hpp"
#include "PostEncoderModuleParams.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <string>
#include <vector>

namespace dl {

/**
 * @brief Extracts the feature vector at a 2D point location within the feature map.
 *
 * Given a point in source-image coordinates, this module:
 *   1. Scales the point from source-image space to feature-map space.
 *   2. Extracts `features[:, :, y_feat, x_feat]` → `[B, C]`.
 *
 * Two interpolation modes are supported:
 *   - **Nearest**: rounds to the nearest integer grid location.
 *   - **Bilinear**: uses `torch::nn::functional::grid_sample` for sub-pixel
 *     accuracy (requires `[B, C, H, W]` 4D input).
 *
 * The current query point must be set via `setPoint()` before each `apply()`.
 *
 * Example:
 * @code
 *     dl::SpatialPointExtractModule extractor(
 *         {640, 480},
 *         dl::SpatialPointModuleParams{.interpolation = dl::InterpolationMode::Bilinear});
 *
 *     extractor.setPoint({320.0f, 240.0f});  // centre pixel
 *     auto vec = extractor.apply(features);  // → [B, C]
 * @endcode
 */
class SpatialPointExtractModule : public PostEncoderModule {
public:
    /**
     * @brief Construct a spatial point extraction module.
     *
     * @param source_image_size Dimensions of the original source image
     *        (before any encoding/resizing). Used for coordinate scaling.
     * @param params User-configurable module parameters (interpolation, point_key).
     *
     * @pre source_image_size.width > 0
     * @pre source_image_size.height > 0
     */
    explicit SpatialPointExtractModule(
            ImageSize source_image_size,
            SpatialPointModuleParams params = {});

    [[nodiscard]] std::string name() const override;

    /**
     * @brief Extract the feature vector at the current query point.
     *
     * @param features Input tensor of shape `[B, C, H, W]`.
     * @return Tensor of shape `[B, C]`.
     *
     * @pre features is defined
     * @pre features.dim() == 4 (layout `[B, C, H, W]`)
     * @pre features.size(0) >= 1
     * @pre features.size(1) >= 1
     * @pre features.size(2) >= 1 and features.size(3) >= 1
     * @pre for bilinear mode: features.size(2) >= 2 and features.size(3) >= 2
     * @pre setPoint() should have been called before apply(); default `{0, 0}` is used otherwise
     */
    [[nodiscard]] at::Tensor apply(at::Tensor const & features) const override;

    /**
     * @brief Returns `{C}` where C is `input_shape[0]`.
     *
     * @param input_shape Encoder output shape `[C, H, W]` excluding the batch dimension.
     *
     * @pre input_shape is not empty
     * @pre input_shape[0] >= 1
     */
    [[nodiscard]] std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const override;

    /**
     * @brief Update the query point (in source-image coordinates) for the next frame.
     *
     * @param point Query location in pixel coordinates of the source image.
     *              (0, 0) is the top-left corner.
     */
    void setPoint(Point2D<float> point);

    /**
     * @brief The current query point in source-image coordinates.
     */
    [[nodiscard]] Point2D<float> currentPoint() const { return _current_point; }

    /**
     * @brief The source image size used for coordinate scaling.
     */
    [[nodiscard]] ImageSize sourceImageSize() const { return _source_image_size; }

    /**
     * @brief The interpolation mode.
     */
    [[nodiscard]] InterpolationMode interpolationMode() const { return _mode; }

    /**
     * @brief DataManager key for per-frame PointData lookup.
     */
    [[nodiscard]] std::string const & pointKey() const { return _point_key; }

private:
    ImageSize _source_image_size;
    InterpolationMode _mode;
    std::string _point_key;
    Point2D<float> _current_point{0.0f, 0.0f};

    /**
     * @brief Nearest-neighbor extraction.
     */
    [[nodiscard]] at::Tensor _extractNearest(at::Tensor const & features) const;

    /**
     * @brief Bilinear interpolation extraction using grid_sample.
     */
    [[nodiscard]] at::Tensor _extractBilinear(at::Tensor const & features) const;
};

}// namespace dl

#endif// NEURALYZER_SPATIAL_POINT_EXTRACT_MODULE_HPP

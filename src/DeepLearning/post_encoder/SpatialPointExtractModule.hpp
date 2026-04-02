/// @file SpatialPointExtractModule.hpp
/// @brief Point-based spatial feature extraction post-encoder module.

#ifndef WHISKERTOOLBOX_SPATIAL_POINT_EXTRACT_MODULE_HPP
#define WHISKERTOOLBOX_SPATIAL_POINT_EXTRACT_MODULE_HPP

#include "PostEncoderModule.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <string>
#include <vector>

namespace dl {

/// Interpolation strategy for spatial feature extraction.
enum class InterpolationMode {
    Nearest,  ///< Round to nearest grid location (fast)
    Bilinear  ///< Sub-pixel bilinear interpolation via grid_sample (accurate)
};

/// Extracts the feature vector at a 2D point location within the feature map.
///
/// Given a point in source-image coordinates, this module:
///   1. Scales the point from source-image space to feature-map space.
///   2. Extracts `features[:, :, y_feat, x_feat]` → `[B, C]`.
///
/// Two interpolation modes are supported:
///   - **Nearest**: rounds to the nearest integer grid location.
///   - **Bilinear**: uses `torch::nn::functional::grid_sample` for sub-pixel
///     accuracy (requires `[B, C, H, W]` 4D input).
///
/// The current query point must be set via `setPoint()` before each `apply()`.
///
/// Example:
/// @code
///     dl::SpatialPointExtractModule extractor(
///         {640, 480},                     // source image size
///         dl::InterpolationMode::Bilinear);
///
///     extractor.setPoint({320.0f, 240.0f});  // centre pixel
///     auto vec = extractor.apply(features);  // → [B, C]
/// @endcode
class SpatialPointExtractModule : public PostEncoderModule {
public:
    /// @param source_image_size  Dimensions of the original source image
    ///        (before any encoding/resizing). Used for coordinate scaling.
    /// @param mode               Interpolation strategy (Nearest or Bilinear).
    ///
    /// @pre source_image_size.width > 0 (enforcement: assert)
    /// @pre source_image_size.height > 0 (enforcement: assert)
    explicit SpatialPointExtractModule(
            ImageSize source_image_size,
            InterpolationMode mode = InterpolationMode::Nearest);

    [[nodiscard]] std::string name() const override;

    /// Extract the feature vector at the current query point.
    ///
    /// @param features  Input tensor of shape `[B, C, H, W]`.
    /// @return Tensor of shape `[B, C]`.
    ///
    /// @pre features must be defined (not a default-constructed undefined tensor) (enforcement: assert)
    /// @pre features.dim() == 4 (enforcement: exception)
    /// @pre features.size(0) >= 1 (batch dim B >= 1) (enforcement: none) [IMPORTANT]
    /// @pre features.size(2) >= 1 and features.size(3) >= 1 (H and W >= 1; bilinear path divides by W-1 and H-1) (enforcement: none) [CRITICAL]
    /// @pre setPoint() should have been called with a meaningful point before apply(); default {0,0} is used otherwise (enforcement: none) [IMPORTANT]
    [[nodiscard]] at::Tensor apply(at::Tensor const & features) const override;

    /// Returns `{C}` where C is `input_shape[0]`.
    ///
    /// @pre input_shape must not be empty (enforcement: assert)
    /// @pre input_shape[0] >= 1 (channel count C must be positive) (enforcement: none) [IMPORTANT]
    [[nodiscard]] std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const override;

    /// Update the query point (in source-image coordinates) for the next frame.
    ///
    /// @param point  Query location in pixel coordinates of the source image.
    ///               (0, 0) is the top-left corner.
    void setPoint(Point2D<float> point);

    /// @return The current query point in source-image coordinates.
    [[nodiscard]] Point2D<float> currentPoint() const { return _current_point; }

    /// @return The source image size used for coordinate scaling.
    [[nodiscard]] ImageSize sourceImageSize() const { return _source_image_size; }

    /// @return The interpolation mode.
    [[nodiscard]] InterpolationMode interpolationMode() const { return _mode; }

private:
    ImageSize _source_image_size;
    InterpolationMode _mode;
    Point2D<float> _current_point{0.0f, 0.0f};

    /// Nearest-neighbor extraction.
    [[nodiscard]] at::Tensor _extractNearest(at::Tensor const & features) const;

    /// Bilinear interpolation extraction using grid_sample.
    [[nodiscard]] at::Tensor _extractBilinear(at::Tensor const & features) const;
};

}// namespace dl

#endif// WHISKERTOOLBOX_SPATIAL_POINT_EXTRACT_MODULE_HPP

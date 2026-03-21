/// @file SpatialPointExtractModule.cpp
/// @brief Implementation of the spatial point feature extraction module.

#include "SpatialPointExtractModule.hpp"

#include <torch/torch.h>

#include <cassert>
#include <stdexcept>

namespace dl {

SpatialPointExtractModule::SpatialPointExtractModule(
        ImageSize source_image_size,
        InterpolationMode mode)
    : _source_image_size{source_image_size}, _mode{mode} {
    assert(source_image_size.width > 0 &&
           "SpatialPointExtractModule: source_image_size.width must be > 0");
    assert(source_image_size.height > 0 &&
           "SpatialPointExtractModule: source_image_size.height must be > 0");
}

std::string SpatialPointExtractModule::name() const {
    return "spatial_point";
}

void SpatialPointExtractModule::setPoint(Point2D<float> point) {
    _current_point = point;
}

at::Tensor SpatialPointExtractModule::apply(at::Tensor const & features) const {
    assert(features.defined() && "SpatialPointExtractModule: features must be defined");
    if (features.dim() != 4) {
        throw std::invalid_argument(
                "SpatialPointExtractModule: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(features.dim()));
    }

    if (_mode == InterpolationMode::Bilinear) {
        return _extractBilinear(features);
    }
    return _extractNearest(features);
}

std::vector<int64_t>
SpatialPointExtractModule::outputShape(
        std::vector<int64_t> const & input_shape) const {
    assert(!input_shape.empty() &&
           "SpatialPointExtractModule: input_shape must not be empty");
    // input_shape is [C, H, W] (excluding batch dim); output is [C]
    return {input_shape[0]};
}

at::Tensor
SpatialPointExtractModule::_extractNearest(at::Tensor const & features) const {
    // features: [B, C, H, W]
    auto const H = static_cast<float>(features.size(2));
    auto const W = static_cast<float>(features.size(3));

    // Scale from source-image coords to feature-map coords
    float const x_feat = _current_point.x *
                         (W / static_cast<float>(_source_image_size.width));
    float const y_feat = _current_point.y *
                         (H / static_cast<float>(_source_image_size.height));

    // Clamp to valid index range
    auto const ix = static_cast<int64_t>(std::round(x_feat));
    auto const iy = static_cast<int64_t>(std::round(y_feat));
    auto const ix_clamped = std::clamp(ix, int64_t{0}, static_cast<int64_t>(W) - 1);
    auto const iy_clamped = std::clamp(iy, int64_t{0}, static_cast<int64_t>(H) - 1);

    // Extract: features[:, :, iy, ix] → [B, C]
    return features.index({torch::indexing::Slice(),
                           torch::indexing::Slice(),
                           iy_clamped,
                           ix_clamped});
}

at::Tensor
SpatialPointExtractModule::_extractBilinear(at::Tensor const & features) const {
    // features: [B, C, H, W]
    auto const H = static_cast<float>(features.size(2));
    auto const W = static_cast<float>(features.size(3));
    auto const B = features.size(0);

    // Scale from source-image coords to feature-map coords
    float const x_feat = _current_point.x *
                         (W / static_cast<float>(_source_image_size.width));
    float const y_feat = _current_point.y *
                         (H / static_cast<float>(_source_image_size.height));

    // Normalise to [-1, 1] for grid_sample
    // grid_sample convention: x in [-1,1] maps to the left/right border pixel
    float const x_norm = 2.0f * x_feat / (W - 1.0f) - 1.0f;
    float const y_norm = 2.0f * y_feat / (H - 1.0f) - 1.0f;

    // Build grid: [B, 1, 1, 2] (out_H=1, out_W=1, xy)
    auto grid = torch::full({B, 1, 1, 2}, 0.0f, features.options());
    grid.index_put_(
            {torch::indexing::Slice(), 0, 0, 0},
            torch::full({B}, x_norm, features.options()));
    grid.index_put_(
            {torch::indexing::Slice(), 0, 0, 1},
            torch::full({B}, y_norm, features.options()));

    namespace F = torch::nn::functional;
    auto opts = F::GridSampleFuncOptions()
                        .mode(torch::kBilinear)
                        .padding_mode(torch::kBorder)
                        .align_corners(true);

    // grid_sample output: [B, C, 1, 1]
    auto sampled = F::grid_sample(features, grid, opts);

    // Squeeze spatial dims → [B, C]
    return sampled.squeeze(-1).squeeze(-1);
}

}// namespace dl

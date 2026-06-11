/**
 * @file SpatialPointExtractModule.cpp
 * @brief Implementation of the spatial point feature extraction module.
 */

#include "SpatialPointExtractModule.hpp"

#include <ATen/Functions.h>  // at::full, at::index_put_
#include <ATen/core/Tensor.h>// at::Tensor
#include <spdlog/spdlog.h>
#include <torch/enum.h>                // torch::kBilinear, torch::kBorder
#include <torch/nn/functional/vision.h>// grid_sample and GridSampleFuncOptions

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dl {

namespace {

/**
 * @brief Validate constructor arguments for spatial point extraction.
 */
void validateSpatialPointExtractConstruction(ImageSize const & source_image_size) {
    if (source_image_size.width <= 0 || source_image_size.height <= 0) {
        auto const message =
                "SpatialPointExtractModule: source_image_size must be positive, got " +
                std::to_string(source_image_size.width) + "x" +
                std::to_string(source_image_size.height);
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }
}

/**
 * @brief Validate input tensor shape for spatial point extraction.
 *
 * @pre features is defined (enforced via assert)
 */
void validateSpatialPointExtractInput(at::Tensor const & features, InterpolationMode mode) {
    assert(features.defined() && "SpatialPointExtractModule: features must be defined");

    if (features.dim() != 4) {
        auto const message =
                "SpatialPointExtractModule: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(features.dim());
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }

    auto const batch_size = features.size(0);
    auto const channel_count = features.size(1);
    auto const height = features.size(2);
    auto const width = features.size(3);

    if (batch_size < 1) {
        auto const message = "SpatialPointExtractModule: batch dimension must be >= 1, got " +
                             std::to_string(batch_size);
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }

    if (channel_count < 1) {
        auto const message =
                "SpatialPointExtractModule: channel dimension must be >= 1, got " +
                std::to_string(channel_count);
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }

    if (height < 1 || width < 1) {
        auto const message =
                "SpatialPointExtractModule: spatial dimensions must be >= 1, got H=" +
                std::to_string(height) + ", W=" + std::to_string(width);
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }

    if (mode == InterpolationMode::Bilinear && (height < 2 || width < 2)) {
        auto const message =
                "SpatialPointExtractModule: bilinear interpolation requires H >= 2 and W >= 2, "
                "got H=" +
                std::to_string(height) + ", W=" + std::to_string(width);
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }
}

/**
 * @brief Validate encoder output shape metadata for spatial point extraction.
 *
 * @pre input_shape is not empty (enforced via assert)
 */
void validateSpatialPointExtractOutputShapeInput(std::vector<int64_t> const & input_shape) {
    assert(!input_shape.empty() && "SpatialPointExtractModule: input_shape must not be empty");

    if (input_shape[0] < 1) {
        auto const message =
                "SpatialPointExtractModule: channel dimension must be >= 1, got " +
                std::to_string(input_shape[0]);
        spdlog::debug("[SpatialPointExtractModule] {}", message);
        throw std::invalid_argument(message);
    }
}

}// namespace

SpatialPointExtractModule::SpatialPointExtractModule(
        ImageSize source_image_size,
        InterpolationMode mode)
    : _source_image_size{source_image_size},
      _mode{mode} {
    validateSpatialPointExtractConstruction(source_image_size);
}

std::string SpatialPointExtractModule::name() const {
    return "spatial_point";
}

void SpatialPointExtractModule::setPoint(Point2D<float> point) {
    _current_point = point;
}

at::Tensor SpatialPointExtractModule::apply(at::Tensor const & features) const {
    validateSpatialPointExtractInput(features, _mode);

    if (_mode == InterpolationMode::Bilinear) {
        return _extractBilinear(features);
    }
    return _extractNearest(features);
}

std::vector<int64_t>
SpatialPointExtractModule::outputShape(
        std::vector<int64_t> const & input_shape) const {
    validateSpatialPointExtractOutputShapeInput(input_shape);
    return {input_shape[0]};
}

at::Tensor
SpatialPointExtractModule::_extractNearest(at::Tensor const & features) const {
    auto const H = static_cast<float>(features.size(2));
    auto const W = static_cast<float>(features.size(3));

    float const x_feat = _current_point.x *
                         (W / static_cast<float>(_source_image_size.width));
    float const y_feat = _current_point.y *
                         (H / static_cast<float>(_source_image_size.height));

    auto const ix = static_cast<int64_t>(std::round(x_feat));
    auto const iy = static_cast<int64_t>(std::round(y_feat));
    auto const ix_clamped = std::clamp(ix, int64_t{0}, static_cast<int64_t>(W) - 1);
    auto const iy_clamped = std::clamp(iy, int64_t{0}, static_cast<int64_t>(H) - 1);

    return features.index({at::indexing::Slice(),
                           at::indexing::Slice(),
                           iy_clamped,
                           ix_clamped});
}

at::Tensor
SpatialPointExtractModule::_extractBilinear(at::Tensor const & features) const {
    auto const H = static_cast<float>(features.size(2));
    auto const W = static_cast<float>(features.size(3));
    auto const B = features.size(0);

    float const x_feat = _current_point.x *
                         (W / static_cast<float>(_source_image_size.width));
    float const y_feat = _current_point.y *
                         (H / static_cast<float>(_source_image_size.height));

    float const x_norm = 2.0f * x_feat / (W - 1.0f) - 1.0f;
    float const y_norm = 2.0f * y_feat / (H - 1.0f) - 1.0f;

    auto grid = at::full({B, 1, 1, 2}, 0.0f, features.options());
    grid.index_put_(
            {at::indexing::Slice(), 0, 0, 0},
            at::full({B}, x_norm, features.options()));
    grid.index_put_(
            {at::indexing::Slice(), 0, 0, 1},
            at::full({B}, y_norm, features.options()));

    namespace F = torch::nn::functional;
    auto opts = F::GridSampleFuncOptions()
                        .mode(torch::kBilinear)
                        .padding_mode(torch::kBorder)
                        .align_corners(true);

    auto sampled = F::grid_sample(features, grid, opts);
    return sampled.squeeze(-1).squeeze(-1);
}

}// namespace dl

/// @file GlobalAvgPoolModule.hpp
/// @brief Global average pooling post-encoder module.

#ifndef WHISKERTOOLBOX_GLOBAL_AVG_POOL_MODULE_HPP
#define WHISKERTOOLBOX_GLOBAL_AVG_POOL_MODULE_HPP

#include "PostEncoderModule.hpp"

#include <string>
#include <vector>

namespace dl {

/// Reduces a spatial feature map `[B, C, H, W]` to a global descriptor `[B, C]`
/// by averaging over all spatial positions.
///
/// This is the standard approach for converting a convolutional feature map into
/// a compact fixed-size embedding suitable for classification or retrieval.
///
/// Implementation: `adaptive_avg_pool2d(features, {1,1}).squeeze(-1).squeeze(-1)`
///
/// Example:
/// @code
///     dl::GlobalAvgPoolModule pool;
///     // features: [1, 384, 7, 7]
///     auto pooled = pool.apply(features);  // → [1, 384]
/// @endcode
class GlobalAvgPoolModule : public PostEncoderModule {
public:
    [[nodiscard]] std::string name() const override;

    /// Pool spatial dimensions to a single global average per channel.
    ///
    /// @param features Input tensor of shape `[B, C, H, W]`.
    /// @return Tensor of shape `[B, C]`.
    ///
    /// @pre features.dim() == 4
    [[nodiscard]] at::Tensor apply(at::Tensor const & features) const override;

    /// Returns `{C}` where C is `input_shape[0]` (the channel dimension).
    ///
    /// @pre input_shape.size() >= 1
    [[nodiscard]] std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const override;
};

}// namespace dl

#endif// WHISKERTOOLBOX_GLOBAL_AVG_POOL_MODULE_HPP

/**
 * @file GlobalAvgPoolModule.hpp
 * @brief Global average pooling post-encoder module.
 */

#ifndef WHISKERTOOLBOX_GLOBAL_AVG_POOL_MODULE_HPP
#define WHISKERTOOLBOX_GLOBAL_AVG_POOL_MODULE_HPP

#include "PostEncoderModule.hpp"

#include <string>
#include <vector>

namespace dl {

/**
 * @brief Reduces a spatial feature map `[B, C, H, W]` to a global descriptor `[B, C]`
 *        by averaging over all spatial positions.
 *
 * This is the standard approach for converting a convolutional feature map into
 * a compact fixed-size embedding suitable for classification or retrieval.
 *
 * Implementation: `adaptive_avg_pool2d(features, {1,1}).squeeze(-1).squeeze(-1)`
 *
 * Example:
 * @code
 *     dl::GlobalAvgPoolModule pool;
 *     // features: [1, 384, 7, 7]
 *     auto pooled = pool.apply(features);  // → [1, 384]
 * @endcode
 */
class GlobalAvgPoolModule : public PostEncoderModule {
public:
    [[nodiscard]] std::string name() const override;

    /**
     * @brief Pool spatial dimensions to a single global average per channel.
     *
     * @param features Input tensor of shape `[B, C, H, W]`.
     * @return Tensor of shape `[B, C]`.
     *
     * @pre features is defined
     * @pre features.dim() == 4 (layout `[B, C, H, W]`)
     * @pre features.size(0) >= 1
     * @pre features.size(1) >= 1
     * @pre features.size(2) >= 1 and features.size(3) >= 1
     */
    [[nodiscard]] at::Tensor apply(at::Tensor const & features) const override;

    /**
     * @brief Returns `{C}` where C is `input_shape[0]` (the channel dimension).
     *
     * @param input_shape Encoder output shape `[C, H, W]` excluding the batch dimension.
     *
     * @pre input_shape is not empty
     * @pre input_shape[0] >= 1
     */
    [[nodiscard]] std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const override;
};

}// namespace dl

#endif// WHISKERTOOLBOX_GLOBAL_AVG_POOL_MODULE_HPP

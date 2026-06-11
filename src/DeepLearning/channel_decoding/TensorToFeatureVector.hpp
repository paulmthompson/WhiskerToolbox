#ifndef WHISKERTOOLBOX_TENSOR_TO_FEATURE_VECTOR_HPP
#define WHISKERTOOLBOX_TENSOR_TO_FEATURE_VECTOR_HPP

/**
 * @file TensorToFeatureVector.hpp
 * @brief Decoder that extracts a flat feature vector from a 1D or 2D tensor.
 */

#include "ChannelDecoder.hpp"

#include <vector>

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Decodes a `[B, C]` (or `[C]`) feature tensor into a flat `std::vector<float>`.
 *
 * Intended for use after a post-encoder module (e.g. `GlobalAvgPoolModule`) has reduced a
 * spatial feature map to a global descriptor. Extracts the feature vector for a single
 * batch element.
 *
 * Supported input shapes:
 *   - `[C]`    — single unbatched vector; returns all C values.
 *   - `[B, C]` — batched; returns the vector at `ctx.batch_index`.
 *
 * @code
 *     dl::TensorToFeatureVector dec;
 *     dl::DecoderContext ctx;
 *     ctx.batch_index = 0;
 *     dl::FeatureVectorDecoderParams params;
 *     auto vec = dec.decode(features, ctx, params);
 * @endcode
 */
class TensorToFeatureVector : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /**
     * @brief Extract a flat feature vector from the tensor.
     *
     * @param tensor Input tensor, shape `[C]` or `[B, C]`.
     * @param ctx `ctx.batch_index` selects the batch element for `[B, C]` inputs.
     * @param params Feature vector decoder params (currently empty).
     * @return Feature values as a flat `std::vector<float>`.
     *
     * @pre tensor is defined
     * @pre tensor.dim() == 1 (layout `[C]`) or tensor.dim() == 2 (layout `[B, C]`)
     * @pre for 2D inputs: 0 <= ctx.batch_index < tensor.size(0)
     *
     * @post Returned vector length equals tensor.size(-1).
     */
    [[nodiscard]] static std::vector<float> decode(at::Tensor const & tensor,
                                                   DecoderContext const & ctx,
                                                   FeatureVectorDecoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_TENSOR_TO_FEATURE_VECTOR_HPP

/**
 * @file PostEncoderModule.hpp
 * @brief Abstract interface for post-encoder feature processing modules.
 *
 * Post-encoder modules are the **tensor-to-tensor** stage of the inference pipeline.
 * They transform a backbone encoder's spatial feature map (`at::Tensor`, typically
 * `[B, C, H, W]`) into a more compact tensor representation still in the torch
 * domain — for example a global descriptor `[B, C]` from `GlobalAvgPoolModule` or
 * a point-sampled vector from `SpatialPointExtractModule`.
 *
 * Typical pipeline position:
 * @code
 *   encoder output [B, C, H, W]
 *        → PostEncoderModule::apply()              // torch → torch
 *        → TensorToFeatureVector::decode() / …     // torch → DataManager type
 *        → DataManager::setData / addAtTime
 * @endcode
 *
 * Post-encoder modules are optional. When omitted, channel decoders operate directly
 * on the raw encoder output (required for spatial decoders such as `TensorToMask2D`).
 * Modules chain via `PostEncoderPipeline` and are instantiated through
 * `PostEncoderModuleFactory`.
 *
 * @see ChannelDecoder.hpp for the final torch-to-DataManager decoding stage
 * @see PostEncoderModuleFactory.hpp for module registration and lookup
 * @see PostEncoderPipeline.hpp for chaining multiple modules
 */

#ifndef WHISKERTOOLBOX_POST_ENCODER_MODULE_HPP
#define WHISKERTOOLBOX_POST_ENCODER_MODULE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Abstract base class for tensor-to-tensor post-encoder processing.
 *
 * Implementations expose `apply()` to transform feature tensors and `outputShape()`
 * to propagate rank/shape metadata without running inference. Modules are stateless
 * by default; stateful modules (e.g. `SpatialPointExtractModule`) expose setters to
 * update per-frame query state before `apply()`.
 *
 * @see ChannelDecoder for the complementary stage that converts torch outputs into
 *      DataManager-compatible types
 *
 * Usage:
 * @code
 *     dl::GlobalAvgPoolModule pool;
 *     at::Tensor features = encoder.forward(inputs)["features"];
 *     at::Tensor pooled = pool.apply(features);  // [B, C, H, W] → [B, C]
 * @endcode
 */
class PostEncoderModule {
public:
    virtual ~PostEncoderModule() = default;

    /**
     * @brief Human-readable identifier for this module (e.g. "global_avg_pool").
     */
    [[nodiscard]] virtual std::string name() const = 0;

    /**
     * @brief Apply the module to a feature tensor.
     *
     * @param features Input tensor, typically `[B, C, H, W]`.
     * @return Transformed tensor.
     *
     * @pre features must not be undefined.
     */
    [[nodiscard]] virtual at::Tensor apply(at::Tensor const & features) const = 0;

    /**
     * @brief Compute the output shape given an input shape (excluding batch dim).
     *
     * @param input_shape Input feature shape (excluding batch dim),
     *        e.g. `{384, 7, 7}` for a `[B, 384, 7, 7]` spatial feature map.
     * @return Output shape (excluding batch dim).
     */
    [[nodiscard]] virtual std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const = 0;
};

}// namespace dl

#endif// WHISKERTOOLBOX_POST_ENCODER_MODULE_HPP

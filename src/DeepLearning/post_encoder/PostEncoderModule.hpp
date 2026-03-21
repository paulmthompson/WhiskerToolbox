/// @file PostEncoderModule.hpp
/// @brief Abstract interface for post-encoder feature processing modules.

#ifndef WHISKERTOOLBOX_POST_ENCODER_MODULE_HPP
#define WHISKERTOOLBOX_POST_ENCODER_MODULE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace at { class Tensor; }

namespace dl {

/// Abstract base class for modules that post-process encoder feature tensors.
///
/// Post-encoder modules transform the raw spatial feature tensor produced by
/// a backbone encoder (e.g. `[B, C, H, W]`) into a more compact representation
/// — for example, a global feature descriptor `[B, C]`.
///
/// Modules are stateless by default. Stateful modules (e.g.
/// `SpatialPointExtractModule`) expose additional setter methods to update
/// their state between frames.
///
/// Usage:
/// @code
///     dl::GlobalAvgPoolModule pool;
///     torch::Tensor features = encoder.forward(inputs)["features"];
///     torch::Tensor pooled = pool.apply(features);  // [B, C, H, W] → [B, C]
/// @endcode
class PostEncoderModule {
public:
    virtual ~PostEncoderModule() = default;

    /// Human-readable identifier for this module (e.g. "global_avg_pool").
    [[nodiscard]] virtual std::string name() const = 0;

    /// Apply the module to a feature tensor.
    ///
    /// @param features Input tensor, typically `[B, C, H, W]`.
    /// @return Transformed tensor.
    ///
    /// @pre features must not be undefined.
    [[nodiscard]] virtual at::Tensor apply(at::Tensor const & features) const = 0;

    /// Compute the output shape given an input shape (excluding batch dim).
    ///
    /// @param input_shape Input feature shape (excluding batch dim),
    ///        e.g. `{384, 7, 7}` for a `[B, 384, 7, 7]` spatial feature map.
    /// @return Output shape (excluding batch dim).
    [[nodiscard]] virtual std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const = 0;
};

}// namespace dl

#endif// WHISKERTOOLBOX_POST_ENCODER_MODULE_HPP

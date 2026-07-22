/**
 * @file PostEncoderPipeline.hpp
 * @brief Sequential pipeline of PostEncoderModule instances.
 */

#ifndef NEURALYZER_POST_ENCODER_PIPELINE_HPP
#define NEURALYZER_POST_ENCODER_PIPELINE_HPP

#include "PostEncoderModule.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief A composable, ordered sequence of `PostEncoderModule` operations.
 *
 * Each module's output is fed as input to the next module in the pipeline.
 * The pipeline is itself a `PostEncoderModule` so it can be stored wherever
 * a single module is expected.
 *
 * Example:
 * @code
 *     dl::PostEncoderPipeline pipeline;
 *     pipeline.add(std::make_unique<dl::GlobalAvgPoolModule>());
 *     // features [B,C,H,W] → [B,C] after pooling
 *     auto result = pipeline.apply(features);
 * @endcode
 */
class PostEncoderPipeline : public PostEncoderModule {
public:
    PostEncoderPipeline() = default;

    /**
     * @brief Append a module to the end of the pipeline.
     *
     * @pre module must not be nullptr
     */
    void add(std::unique_ptr<PostEncoderModule> module);

    /**
     * @brief Number of stages in the pipeline.
     */
    [[nodiscard]] std::size_t size() const { return _modules.size(); }

    /**
     * @brief Whether the pipeline contains no stages.
     */
    [[nodiscard]] bool empty() const { return _modules.empty(); }

    // PostEncoderModule interface
    [[nodiscard]] std::string name() const override;

    /**
     * @brief Apply each module sequentially, passing the output of each as the
     *        input to the next.
     *
     * @pre features must not be undefined.
     */
    [[nodiscard]] at::Tensor apply(at::Tensor const & features) const override;

    /**
     * @brief Compute the output shape by propagating through each module's
     *        `outputShape()` in order.
     */
    [[nodiscard]] std::vector<int64_t>
    outputShape(std::vector<int64_t> const & input_shape) const override;

private:
    std::vector<std::unique_ptr<PostEncoderModule>> _modules;
};

}// namespace dl

#endif// NEURALYZER_POST_ENCODER_PIPELINE_HPP

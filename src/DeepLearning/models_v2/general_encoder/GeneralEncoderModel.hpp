/// @file GeneralEncoderModel.hpp
/// @brief A general-purpose encoder model wrapper for image feature extraction.

#ifndef WHISKERTOOLBOX_GENERAL_ENCODER_MODEL_HPP
#define WHISKERTOOLBOX_GENERAL_ENCODER_MODEL_HPP

#include "models_v2/ModelBase.hpp"
#include "models_v2/ModelExecution.hpp"
#include "post_encoder/PostEncoderModule.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/// General-purpose encoder model for extracting spatial feature tensors from images.
///
/// Maps a single input image to a feature tensor using any backbone (e.g. ConvNeXt,
/// ViT, ResNet). The input resolution, number of input channels, and output feature
/// shape are configurable at construction time.
///
/// Input slots:
///   - "image" : [input_channels, height, width] — input image (RGB by default)
///
/// Output slots:
///   - "features" : [output_shape...] — extracted feature tensor
///
/// Batch mode: DynamicBatch{1, max} (supports arbitrary batch sizes).
///
/// @pre Input resolution and output shape must have positive dimensions.
///
/// Usage:
/// @code
///     // Default: 3-channel 224x224 input → 384x7x7 output
///     dl::GeneralEncoderModel model;
///
///     // Custom resolution:
///     dl::GeneralEncoderModel model(3, 512, 512, {768, 16, 16});
/// @endcode
class GeneralEncoderModel : public ModelBase {
public:
    /// Construct with default resolution (3×224×224 input, 384×7×7 output).
    GeneralEncoderModel();

    /// Construct with custom input/output configuration.
    ///
    /// @param input_channels  Number of input channels (e.g. 3 for RGB).
    /// @param input_height    Input image height in pixels.
    /// @param input_width     Input image width in pixels.
    /// @param output_shape    Output feature tensor shape (excluding batch dim).
    ///
    /// @pre input_channels > 0
    /// @pre input_height > 0
    /// @pre input_width > 0
    /// @pre output_shape must not be empty and all dimensions must be > 0
    GeneralEncoderModel(
            int input_channels,
            int input_height,
            int input_width,
            std::vector<int64_t> output_shape);

    ~GeneralEncoderModel() override;

    // Non-copyable, movable
    GeneralEncoderModel(GeneralEncoderModel const &) = delete;
    GeneralEncoderModel & operator=(GeneralEncoderModel const &) = delete;
    GeneralEncoderModel(GeneralEncoderModel &&) noexcept;
    GeneralEncoderModel & operator=(GeneralEncoderModel &&) noexcept;

    [[nodiscard]] std::string modelId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::string description() const override;

    [[nodiscard]] std::vector<TensorSlotDescriptor> inputSlots() const override;
    [[nodiscard]] std::vector<TensorSlotDescriptor> outputSlots() const override;

    void loadWeights(std::filesystem::path const & path) override;
    [[nodiscard]] bool isReady() const override;

    [[nodiscard]] int preferredBatchSize() const override;
    [[nodiscard]] int maxBatchSize() const override;
    [[nodiscard]] dl::BatchMode batchMode() const override;

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & inputs) override;

    // Slot name constants
    static constexpr char const * kImageSlot = "image";
    static constexpr char const * kFeaturesSlot = "features";

    /// @name Configuration accessors
    /// @{
    [[nodiscard]] int inputChannels() const { return _input_channels; }
    [[nodiscard]] int inputHeight() const { return _input_height; }
    [[nodiscard]] int inputWidth() const { return _input_width; }

    /// Raw encoder output shape (before any post-encoder module is applied).
    [[nodiscard]] std::vector<int64_t> const & outputShape() const { return _output_shape; }

    /// Effective output shape reported by `outputSlots()`, accounting for any
    /// configured post-encoder module.
    [[nodiscard]] std::vector<int64_t> effectiveOutputShape() const;
    /// @}

    /// @name Post-encoder module
    /// @{

    /// Replace the current post-encoder module.
    ///
    /// If `module` is `nullptr`, the raw encoder output is returned unchanged.
    /// Calling this method immediately changes the shape reported by
    /// `outputSlots()`.
    ///
    /// @note Not thread-safe; must be called before concurrent `forward()` use.
    void setPostEncoderModule(std::unique_ptr<PostEncoderModule> module);

    /// Non-owning access to the current post-encoder module.
    /// Returns `nullptr` if none is configured.
    [[nodiscard]] PostEncoderModule * postEncoderModule() const;
    /// @}

private:
    int _input_channels;
    int _input_height;
    int _input_width;
    std::vector<int64_t> _output_shape;
    ModelExecution _execution;
    std::vector<std::string> _input_order;
    std::unique_ptr<PostEncoderModule> _post_encoder_module;
};

}// namespace dl

#endif// WHISKERTOOLBOX_GENERAL_ENCODER_MODEL_HPP

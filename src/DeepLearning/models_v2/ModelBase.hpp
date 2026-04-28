#ifndef WHISKERTOOLBOX_MODEL_BASE_HPP
#define WHISKERTOOLBOX_MODEL_BASE_HPP

#include "TensorSlotDescriptor.hpp"

#include <torch/torch.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/// Abstract base class for all v2 deep-learning model wrappers.
///
/// A ModelBase subclass declares its expected input and output tensor slots
/// via `inputSlots()` and `outputSlots()`. The caller allocates and fills
/// input tensors (using ChannelEncoders), invokes `forward()`, and decodes
/// the output tensors (using ChannelDecoders).
///
/// Tensors always have a leading batch dimension. The model advertises
/// `preferredBatchSize()` and `maxBatchSize()` so the UI can choose how
/// many frames to process per call.
class ModelBase {
public:
    virtual ~ModelBase() = default;

    /**
     * @brief Unique string identifier for this model class (e.g. "neurosam").
     */
    [[nodiscard]] virtual std::string modelId() const = 0;

    /**
     * @brief Human-readable display name (e.g. "NeuroSAM").
     */
    [[nodiscard]] virtual std::string displayName() const = 0;

    /**
     * @brief Description for UI tooltip.
     */
    [[nodiscard]] virtual std::string description() const = 0;

    /// Input slot descriptors (ordered). Each describes one named tensor
    /// the model expects. Does not include the batch dimension.
    [[nodiscard]] virtual std::vector<TensorSlotDescriptor> inputSlots() const = 0;

    /// Output slot descriptors (ordered). Each describes one named tensor
    /// the model produces. Does not include the batch dimension.
    [[nodiscard]] virtual std::vector<TensorSlotDescriptor> outputSlots() const = 0;

    /// Load weights from a file (e.g. a `.pte` ExecuTorch program or a TorchScript model).
    virtual void loadWeights(std::filesystem::path const & path) = 0;

    /**
     * @brief Whether weights are loaded and model is ready for inference.
     */
    [[nodiscard]] virtual bool isReady() const = 0;

    /// Preferred batch size. 0 = model decides; 1 = single-frame (e.g.
    /// NeuroSAM with output→input feedback); N = process N frames at once.
    /// UI uses this as the default for the "Batch Size" spinbox.
    [[nodiscard]] virtual int preferredBatchSize() const { return 0; }

    /**
     * @brief Maximum batch size the model supports. 0 = unlimited.
     */
    [[nodiscard]] virtual int maxBatchSize() const { return 0; }

    /// Batch mode describing the model's batch-size constraints.
    ///
    /// The default returns `DynamicBatch{1, 0}` (any batch size).
    /// Override in subclasses to report `FixedBatch`, `RecurrentOnlyBatch`,
    /// or a `DynamicBatch` with specific limits.
    ///
    /// The widget uses this to configure the batch-size spinbox and to
    /// auto-select the correct weights variant for multi-variant models.
    [[nodiscard]] virtual BatchMode batchMode() const {
        return DynamicBatch{1, maxBatchSize()};
    }

    /// Run inference.
    ///
    /// @param inputs  Map of slot_name → Tensor, each with a leading batch dimension.
    /// @return        Map of slot_name → Tensor for each output slot.
    ///
    /// Tensors have a leading batch dimension whose size is controlled by
    /// the caller (and bounded by `maxBatchSize()`).
    virtual std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & inputs) = 0;
};

}// namespace dl

#endif// WHISKERTOOLBOX_MODEL_BASE_HPP

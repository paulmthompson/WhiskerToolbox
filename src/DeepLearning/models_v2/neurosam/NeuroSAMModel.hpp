#ifndef WHISKERTOOLBOX_NEUROSAM_MODEL_HPP
#define WHISKERTOOLBOX_NEUROSAM_MODEL_HPP

#include "models_v2/ModelBase.hpp"
#include "models_v2/ModelExecution.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/// Concrete ModelBase subclass wrapping the NeuroSAM ExecuTorch model.
///
/// NeuroSAM is a Segment-Anything-style model for neural data that predicts a
/// probability map from a current frame and a set of memory frames. It operates
/// in a feedback loop where the output probability map is fed back as a memory
/// mask for the next frame, requiring single-frame (batch=1) inference.
///
/// Input slots:
///   - "encoder_image"  : [3, 256, 256]  — current video frame (RGB)
///   - "memory_images"  : [3, 256, 256]  — memory encoder frames (static, per-slot)
///   - "memory_masks"   : [1, 256, 256]  — memory ROI masks (static, per-slot)
///
/// Output slots:
///   - "probability_map": [1, 256, 256]  — probability map (decode as mask or point)
///
/// The non-sequence variant treats each memory slot as a separate tensor input.
/// The UI/SlotAssembler handles stacking multiple memory entries.
class NeuroSAMModel : public ModelBase {
public:
    NeuroSAMModel();
    ~NeuroSAMModel() override;

    // Non-copyable, movable
    NeuroSAMModel(NeuroSAMModel const &) = delete;
    NeuroSAMModel & operator=(NeuroSAMModel const &) = delete;
    NeuroSAMModel(NeuroSAMModel &&) noexcept;
    NeuroSAMModel & operator=(NeuroSAMModel &&) noexcept;

    [[nodiscard]] std::string modelId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::string description() const override;

    [[nodiscard]] std::vector<TensorSlotDescriptor> inputSlots() const override;
    [[nodiscard]] std::vector<TensorSlotDescriptor> outputSlots() const override;

    void loadWeights(std::filesystem::path const & path) override;
    [[nodiscard]] bool isReady() const override;

    [[nodiscard]] int preferredBatchSize() const override;
    [[nodiscard]] int maxBatchSize() const override;

    std::unordered_map<std::string, torch::Tensor>
    forward(std::unordered_map<std::string, torch::Tensor> const & inputs) override;

    /// The spatial resolution expected by the model for all image/mask inputs.
    static constexpr int kModelSize = 256;

    /// Number of input channels for image inputs (RGB).
    static constexpr int kImageChannels = 3;

    /// Number of channels for mask inputs.
    static constexpr int kMaskChannels = 1;

    /// Number of channels for the output probability map.
    static constexpr int kOutputChannels = 1;

    // Slot name constants
    static constexpr char const * kEncoderImageSlot = "encoder_image";
    static constexpr char const * kMemoryImagesSlot = "memory_images";
    static constexpr char const * kMemoryMasksSlot = "memory_masks";
    static constexpr char const * kProbabilityMapSlot = "probability_map";

private:
    ModelExecution _execution;
    std::vector<std::string> _input_order;
};

} // namespace dl

#endif // WHISKERTOOLBOX_NEUROSAM_MODEL_HPP

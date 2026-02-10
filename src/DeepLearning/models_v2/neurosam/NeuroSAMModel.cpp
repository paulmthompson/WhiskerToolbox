#include "NeuroSAMModel.hpp"

#include "device/DeviceManager.hpp"
#include "registry/ModelRegistry.hpp"

#include <stdexcept>
#include <utility>

namespace dl {

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------
NeuroSAMModel::NeuroSAMModel()
    : _input_order{kEncoderImageSlot, kMemoryImagesSlot,
                   kMemoryMasksSlot, kMemoryMaskSlot}
{
}

NeuroSAMModel::~NeuroSAMModel() = default;
NeuroSAMModel::NeuroSAMModel(NeuroSAMModel &&) noexcept = default;
NeuroSAMModel & NeuroSAMModel::operator=(NeuroSAMModel &&) noexcept = default;

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------
std::string NeuroSAMModel::modelId() const
{
    return "neurosam";
}

std::string NeuroSAMModel::displayName() const
{
    return "NeuroSAM";
}

std::string NeuroSAMModel::description() const
{
    return "Segment-Anything-style model for neural data. "
           "Predicts a probability map from a current frame and memory frames.";
}

// ---------------------------------------------------------------------------
// Slot descriptors
// ---------------------------------------------------------------------------
std::vector<TensorSlotDescriptor> NeuroSAMModel::inputSlots() const
{
    return {
        {.name = kEncoderImageSlot,
         .shape = {kImageChannels, kModelSize, kModelSize},
         .description = "Current frame",
         .recommended_encoder = "ImageEncoder",
         .recommended_decoder = {},
         .is_static = false,
         .is_boolean_mask = false,
         .sequence_dim = -1},

        {.name = kMemoryImagesSlot,
         .shape = {kImageChannels, kModelSize, kModelSize},
         .description = "Memory encoder frames",
         .recommended_encoder = "ImageEncoder",
         .recommended_decoder = {},
         .is_static = true,
         .is_boolean_mask = false,
         .sequence_dim = -1},

        {.name = kMemoryMasksSlot,
         .shape = {kMaskChannels, kModelSize, kModelSize},
         .description = "Memory ROI masks",
         .recommended_encoder = "Mask2DEncoder",
         .recommended_decoder = {},
         .is_static = true,
         .is_boolean_mask = false,
         .sequence_dim = -1},

        {.name = kMemoryMaskSlot,
         .shape = {1},
         .description = "Memory slot active flags",
         .recommended_encoder = {},
         .recommended_decoder = {},
         .is_static = true,
         .is_boolean_mask = true,
         .sequence_dim = -1},
    };
}

std::vector<TensorSlotDescriptor> NeuroSAMModel::outputSlots() const
{
    return {
        {.name = kProbabilityMapSlot,
         .shape = {kOutputChannels, kModelSize, kModelSize},
         .description = "Output probability map",
         .recommended_encoder = {},
         .recommended_decoder = "TensorToMask2D",
         .is_static = false,
         .is_boolean_mask = false,
         .sequence_dim = -1},
    };
}

// ---------------------------------------------------------------------------
// Weights / readiness
// ---------------------------------------------------------------------------
void NeuroSAMModel::loadWeights(std::filesystem::path const & path)
{
    if (!_execution.load(path)) {
        throw std::runtime_error(
            "NeuroSAMModel::loadWeights(): failed to load weights from " +
            path.string());
    }
}

bool NeuroSAMModel::isReady() const
{
    return _execution.isLoaded();
}

// ---------------------------------------------------------------------------
// Batch size
// ---------------------------------------------------------------------------
int NeuroSAMModel::preferredBatchSize() const
{
    // NeuroSAM uses output→input feedback, so it requires single-frame inference.
    return 1;
}

int NeuroSAMModel::maxBatchSize() const
{
    // Feedback loop requires single-frame processing.
    return 1;
}

// ---------------------------------------------------------------------------
// Forward pass
// ---------------------------------------------------------------------------
std::unordered_map<std::string, torch::Tensor>
NeuroSAMModel::forward(
    std::unordered_map<std::string, torch::Tensor> const & inputs)
{
    if (!isReady()) {
        throw std::runtime_error(
            "NeuroSAMModel::forward(): model not ready (weights not loaded)");
    }

    // Validate required inputs
    for (auto const & slot_name : _input_order) {
        if (inputs.find(slot_name) == inputs.end()) {
            throw std::runtime_error(
                "NeuroSAMModel::forward(): missing required input '" +
                slot_name + "'");
        }
    }

    // Move inputs to the active device
    auto & device_mgr = DeviceManager::instance();
    std::unordered_map<std::string, torch::Tensor> device_inputs;
    device_inputs.reserve(inputs.size());
    for (auto const & [name, tensor] : inputs) {
        device_inputs[name] = device_mgr.toDevice(tensor);
    }

    // Execute via ExecuTorch (uses ordered input convention)
    auto output_tensors = _execution.executeNamed(device_inputs, _input_order);

    // Map outputs to named slots
    std::unordered_map<std::string, torch::Tensor> result;
    if (!output_tensors.empty()) {
        result[kProbabilityMapSlot] = std::move(output_tensors[0]);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Self-registration with ModelRegistry
// ---------------------------------------------------------------------------
DL_REGISTER_MODEL(NeuroSAMModel)

} // namespace dl

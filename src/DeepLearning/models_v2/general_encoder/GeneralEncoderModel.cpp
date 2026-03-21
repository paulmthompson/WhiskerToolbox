/// @file GeneralEncoderModel.cpp
/// @brief Implementation of the general-purpose encoder model wrapper.

#include "GeneralEncoderModel.hpp"

#include "device/DeviceManager.hpp"
#include "registry/ModelRegistry.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>
namespace dl {

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------
GeneralEncoderModel::GeneralEncoderModel()
    : GeneralEncoderModel(3, 224, 224, {384, 7, 7}) {
}

GeneralEncoderModel::GeneralEncoderModel(
        int input_channels,
        int input_height,
        int input_width,
        std::vector<int64_t> output_shape)
    : _input_channels{input_channels},
      _input_height{input_height},
      _input_width{input_width},
      _output_shape{std::move(output_shape)},
      _input_order{kImageSlot} {
    assert(input_channels > 0 && "GeneralEncoderModel: input_channels must be > 0");
    assert(input_height > 0 && "GeneralEncoderModel: input_height must be > 0");
    assert(input_width > 0 && "GeneralEncoderModel: input_width must be > 0");
    assert(!_output_shape.empty() && "GeneralEncoderModel: output_shape must not be empty");
}

GeneralEncoderModel::~GeneralEncoderModel() = default;
GeneralEncoderModel::GeneralEncoderModel(GeneralEncoderModel &&) noexcept = default;
GeneralEncoderModel & GeneralEncoderModel::operator=(GeneralEncoderModel &&) noexcept = default;

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------
std::string GeneralEncoderModel::modelId() const {
    return "general_encoder";
}

std::string GeneralEncoderModel::displayName() const {
    return "General Encoder";
}

std::string GeneralEncoderModel::description() const {
    return "General-purpose image encoder that extracts spatial feature tensors. "
           "Compatible with any backbone (ConvNeXt, ViT, ResNet, etc.).";
}

// ---------------------------------------------------------------------------
// Slot descriptors
// ---------------------------------------------------------------------------
std::vector<TensorSlotDescriptor> GeneralEncoderModel::inputSlots() const {
    return {
            {.name = kImageSlot,
             .shape = {_input_channels, _input_height, _input_width},
             .description = "Input image",
             .recommended_encoder = "ImageEncoder",
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = TensorDType::Float32,
             .sequence_dim = -1},
    };
}

std::vector<TensorSlotDescriptor> GeneralEncoderModel::outputSlots() const {
    return {
            {.name = kFeaturesSlot,
             .shape = effectiveOutputShape(),
             .description = "Extracted feature tensor",
             .recommended_encoder = {},
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = TensorDType::Float32,
             .sequence_dim = -1},
    };
}

// ---------------------------------------------------------------------------
// Weights / readiness
// ---------------------------------------------------------------------------
void GeneralEncoderModel::loadWeights(std::filesystem::path const & path) {
    if (!_execution.load(path)) {
        throw std::runtime_error(
                "GeneralEncoderModel::loadWeights(): failed to load weights from " +
                path.string());
    }
}

bool GeneralEncoderModel::isReady() const {
    return _execution.isLoaded();
}

// ---------------------------------------------------------------------------
// Batch size
// ---------------------------------------------------------------------------
int GeneralEncoderModel::preferredBatchSize() const {
    return 0;// model decides
}

int GeneralEncoderModel::maxBatchSize() const {
    return 0;// unlimited
}

BatchMode GeneralEncoderModel::batchMode() const {
    return DynamicBatch{1, 0};
}

// ---------------------------------------------------------------------------
// Post-encoder module
// ---------------------------------------------------------------------------
std::vector<int64_t> GeneralEncoderModel::effectiveOutputShape() const {
    if (_post_encoder_module) {
        return _post_encoder_module->outputShape(_output_shape);
    }
    return _output_shape;
}

void GeneralEncoderModel::setPostEncoderModule(
        std::unique_ptr<PostEncoderModule> module) {
    _post_encoder_module = std::move(module);
}

PostEncoderModule * GeneralEncoderModel::postEncoderModule() const {
    return _post_encoder_module.get();
}

// ---------------------------------------------------------------------------
// Runtime shape configuration
// ---------------------------------------------------------------------------
void GeneralEncoderModel::setInputResolution(int height, int width) {
    assert(height > 0 && "GeneralEncoderModel::setInputResolution: height must be > 0");
    assert(width > 0 && "GeneralEncoderModel::setInputResolution: width must be > 0");
    _input_height = height;
    _input_width = width;
}

void GeneralEncoderModel::setOutputShape(std::vector<int64_t> output_shape) {
    assert(!output_shape.empty() && "GeneralEncoderModel::setOutputShape: output_shape must not be empty");
    _output_shape = std::move(output_shape);
}

// ---------------------------------------------------------------------------
// Forward pass
// ---------------------------------------------------------------------------
std::unordered_map<std::string, torch::Tensor>
GeneralEncoderModel::forward(
        std::unordered_map<std::string, torch::Tensor> const & inputs) {
    if (!isReady()) {
        throw std::runtime_error(
                "GeneralEncoderModel::forward(): model not ready (weights not loaded)");
    }

    // Validate required inputs
    if (inputs.find(kImageSlot) == inputs.end()) {
        throw std::runtime_error(
                "GeneralEncoderModel::forward(): missing required input '" +
                std::string(kImageSlot) + "'");
    }

    // Move inputs to the active device
    auto & device_mgr = DeviceManager::instance();
    std::unordered_map<std::string, torch::Tensor> device_inputs;
    device_inputs.reserve(inputs.size());
    for (auto const & [name, tensor]: inputs) {
        device_inputs[name] = device_mgr.toDevice(tensor);
    }

    // Execute via the multi-backend execution layer
    auto output_tensors = _execution.executeNamed(device_inputs, _input_order);

    if (output_tensors.empty()) {
        throw std::runtime_error(
                "GeneralEncoderModel::forward(): model returned no outputs");
    }

    // Map the first output to the "features" slot
    std::unordered_map<std::string, torch::Tensor> result;
    torch::Tensor features = output_tensors[0].to(torch::kCPU);

    // Apply post-encoder module if configured
    if (_post_encoder_module) {
        features = _post_encoder_module->apply(features);
    }

    result[kFeaturesSlot] = std::move(features);

    return result;
}

// ---------------------------------------------------------------------------
// Static registration
// ---------------------------------------------------------------------------
DL_REGISTER_MODEL(GeneralEncoderModel)

}// namespace dl

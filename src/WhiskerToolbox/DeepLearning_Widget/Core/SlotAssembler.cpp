#include "SlotAssembler.hpp"

// Use the Qt-free binding data header instead of the full DeepLearningState
// (which inherits from EditorState → QObject → #define slots).
#include "DeepLearningBindingData.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"

#include "channel_encoding/ChannelEncoder.hpp"
#include "channel_encoding/EncoderFactory.hpp"
#include "channel_encoding/ImageEncoder.hpp"
#include "channel_encoding/Point2DEncoder.hpp"
#include "channel_encoding/Mask2DEncoder.hpp"
#include "channel_encoding/Line2DEncoder.hpp"

#include "channel_decoding/ChannelDecoder.hpp"
#include "channel_decoding/DecoderFactory.hpp"
#include "channel_decoding/TensorToPoint2D.hpp"
#include "channel_decoding/TensorToMask2D.hpp"
#include "channel_decoding/TensorToLine2D.hpp"

#include "models_v2/ModelBase.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"
#include "models_v2/TensorDTypeUtils.hpp"
#include "registry/ModelRegistry.hpp"

#include <torch/torch.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════
// PIMPL
// ════════════════════════════════════════════════════════════════════════════

struct SlotAssembler::Impl {
    std::unique_ptr<dl::ModelBase> model;
    std::string model_id;
};

// ════════════════════════════════════════════════════════════════════════════
// Anonymous helpers
// ════════════════════════════════════════════════════════════════════════════

namespace {

dl::RasterMode modeFromString(std::string const & mode_str) {
    if (mode_str == "Binary") return dl::RasterMode::Binary;
    if (mode_str == "Heatmap") return dl::RasterMode::Heatmap;
    if (mode_str == "Distance") return dl::RasterMode::Distance;
    return dl::RasterMode::Raw;
}

/// Return the default/required mode for a given encoder type.
/// Some encoders (like Mask2DEncoder) only support one mode.
std::string defaultModeForEncoder(std::string const & encoder_id) {
    if (encoder_id == "Mask2DEncoder") return "Binary";
    if (encoder_id == "Line2DEncoder") return "Binary";
    if (encoder_id == "Point2DEncoder") return "Binary";
    return "Raw"; // ImageEncoder default
}

dl::TensorSlotDescriptor const *
findSlot(std::vector<dl::TensorSlotDescriptor> const & slot_vec,
         std::string const & name) {
    for (auto const & s : slot_vec) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

dl::EncoderParams makeEncoderParams(
    dl::TensorSlotDescriptor const & slot,
    SlotBindingData const & binding,
    int batch_index) {

    dl::EncoderParams params;
    params.target_channel = 0;
    params.batch_index = batch_index;
    params.mode = modeFromString(binding.mode);
    params.gaussian_sigma = binding.gaussian_sigma;

    if (slot.shape.size() >= 2) {
        params.height = static_cast<int>(slot.shape[slot.shape.size() - 2]);
        params.width = static_cast<int>(slot.shape[slot.shape.size() - 1]);
    } else if (slot.shape.size() == 1) {
        params.height = 1;
        params.width = static_cast<int>(slot.shape[0]);
    }

    return params;
}

/// Encode a dynamic (per-frame) input slot into the tensor.
///
/// @param dm DataManager to fetch data from
/// @param binding Slot binding configuration
/// @param slot Tensor slot descriptor
/// @param tensor Output tensor to write to
/// @param frame Frame number to encode
/// @param batch_index Batch index in the tensor
/// @param source_image_size Original image dimensions (for coordinate scaling of masks/points/lines)
void encodeDynamicSlot(
    DataManager & dm,
    SlotBindingData const & binding,
    dl::TensorSlotDescriptor const & slot,
    torch::Tensor & tensor,
    int frame,
    int batch_index,
    ImageSize const & source_image_size) {

    auto params = makeEncoderParams(slot, binding, batch_index);

    if (binding.encoder_id == "ImageEncoder") {
        auto media = dm.getData<MediaData>(binding.data_key);
        if (!media) {
            std::cerr << "SlotAssembler: MediaData not found for key '"
                      << binding.data_key << "'\n";
            return;
        }
        media->LoadFrame(frame);

        int const channels =
            (media->getFormat() == MediaData::DisplayFormat::Color) ? 3 : 1;
        auto const image_size = media->getImageSize();

        dl::ImageEncoder encoder;
        if (media->is8Bit()) {
            auto const & data = media->getRawData8(frame);
            encoder.encode(data, image_size, channels, tensor, params);
        } else {
            auto const & data = media->getRawData32(frame);
            encoder.encode(data, image_size, channels, tensor, params);
        }

    } else if (binding.encoder_id == "Point2DEncoder") {
        auto point_data = dm.getData<PointData>(binding.data_key);
        if (!point_data) return;
        // Use the original image size for coordinate scaling, not model input size
        ImageSize const & actual_source = (source_image_size.width > 0 && source_image_size.height > 0)
            ? source_image_size
            : ImageSize{params.width, params.height};
        dl::Point2DEncoder encoder;
        
        // Get points at the requested frame
        auto points_at_frame = point_data->getAtTime(TimeFrameIndex(frame));
        std::vector<Point2D<float>> points_vec;
        for (auto const & pt : points_at_frame) {
            points_vec.push_back(pt);
        }
        encoder.encode(points_vec, actual_source, tensor, params);

    } else if (binding.encoder_id == "Mask2DEncoder") {
        auto mask_data = dm.getData<MaskData>(binding.data_key);
        if (!mask_data) return;
        // Use the original image size for coordinate scaling, not model input size
        ImageSize const & actual_source = (source_image_size.width > 0 && source_image_size.height > 0)
            ? source_image_size
            : ImageSize{params.width, params.height};
        dl::Mask2DEncoder encoder;
        
        // Get masks at the requested frame, use the first one if available
        auto masks_at_frame = mask_data->getAtTime(TimeFrameIndex(frame));
        Mask2D mask_to_encode;
        for (auto const & m : masks_at_frame) {
            mask_to_encode = m;  // Use the first mask found
            break;
        }
        encoder.encode(mask_to_encode, actual_source, tensor, params);

    } else if (binding.encoder_id == "Line2DEncoder") {
        auto line_data = dm.getData<LineData>(binding.data_key);
        if (!line_data) return;
        // Use the original image size for coordinate scaling, not model input size
        ImageSize const & actual_source = (source_image_size.width > 0 && source_image_size.height > 0)
            ? source_image_size
            : ImageSize{params.width, params.height};
        dl::Line2DEncoder encoder;
        
        // Get lines at the requested frame, use the first one if available
        auto lines_at_frame = line_data->getAtTime(TimeFrameIndex(frame));
        Line2D line_to_encode;
        for (auto const & l : lines_at_frame) {
            line_to_encode = l;  // Use the first line found
            break;
        }
        encoder.encode(line_to_encode, actual_source, tensor, params);

    } else {
        std::cerr << "SlotAssembler: unknown encoder '" << binding.encoder_id
                  << "' for slot '" << binding.slot_name << "'\n";
    }
}

std::unordered_map<std::string, torch::Tensor>
assembleInputs(
    DataManager & dm,
    dl::ModelBase const & model,
    std::vector<SlotBindingData> const & input_bindings,
    std::vector<StaticInputData> const & static_inputs,
    int current_frame,
    int batch_size) {

    std::unordered_map<std::string, torch::Tensor> result;
    auto const input_slot_vec = model.inputSlots();

    // ── Detect source image size from ImageEncoder binding ──
    // Masks, points, and lines are stored in original image coordinates,
    // so we need to know the original image dimensions for proper scaling.
    ImageSize source_image_size{0, 0};
    for (auto const & binding : input_bindings) {
        if (binding.encoder_id == "ImageEncoder" && !binding.data_key.empty()) {
            auto media = dm.getData<MediaData>(binding.data_key);
            if (media) {
                source_image_size = media->getImageSize();
                break;
            }
        }
    }

    // ── Dynamic (per-frame) inputs ──
    for (auto const & binding : input_bindings) {
        auto const * slot = findSlot(input_slot_vec, binding.slot_name);
        if (!slot || binding.data_key.empty()) continue;

        std::vector<int64_t> tensor_shape;
        tensor_shape.push_back(batch_size);
        tensor_shape.insert(tensor_shape.end(),
                            slot->shape.begin(), slot->shape.end());

        // Use dtype from slot descriptor (model specifies expected dtype)
        auto tensor = torch::zeros(tensor_shape, toTorchDType(slot->dtype));
        for (int b = 0; b < batch_size; ++b) {
            encodeDynamicSlot(dm, binding, *slot, tensor,
                              current_frame + b, b, source_image_size);
        }
        result[binding.slot_name] = std::move(tensor);
    }

    // ── Static (memory) inputs ──
    std::unordered_map<std::string, std::vector<StaticInputData const *>> grouped;
    for (auto const & si : static_inputs) {
        grouped[si.slot_name].push_back(&si);
    }

    // Process all static/memory input slots (including boolean masks)
    for (auto const & slot : input_slot_vec) {
        if (!slot.is_static) continue; // skip dynamic inputs

        auto const & entries = grouped[slot.name]; // may be empty

        if (slot.is_boolean_mask) {
            // Boolean mask: always create, even if no entries
            std::vector<int64_t> shape = {batch_size};
            for (auto d : slot.shape) shape.push_back(d);
            auto tensor = torch::zeros(shape, toTorchDType(slot.dtype));
            for (auto const * entry : entries) {
                if (entry->active &&
                    entry->memory_index < static_cast<int>(slot.shape[0])) {
                    tensor[0][entry->memory_index] = 1.0f;
                }
            }
            result[slot.name] = std::move(tensor);
        } else if (!entries.empty()) {
            // Memory frame slots: pull from DataManager at time offsets
            // Only create if user has configured at least one entry
            std::vector<int64_t> tensor_shape = {batch_size};
            tensor_shape.insert(tensor_shape.end(),
                                slot.shape.begin(), slot.shape.end());
            // Use dtype from slot descriptor
            auto tensor = torch::zeros(tensor_shape, toTorchDType(slot.dtype));
            for (auto const * entry : entries) {
                if (entry->data_key.empty()) continue;
                int const frame = std::max(0,
                    current_frame + entry->time_offset);
                SlotBindingData temp_binding;
                temp_binding.slot_name = slot.name;
                temp_binding.data_key = entry->data_key;
                temp_binding.encoder_id = slot.recommended_encoder;
                // Use appropriate default mode for this encoder type
                temp_binding.mode = defaultModeForEncoder(slot.recommended_encoder);
                temp_binding.gaussian_sigma = 2.0f;
                encodeDynamicSlot(dm, temp_binding, slot, tensor, frame, 0, source_image_size);
            }
            result[slot.name] = std::move(tensor);
        }
    }

    return result;
}

void decodeOutputs(
    DataManager & dm,
    std::unordered_map<std::string, torch::Tensor> const & outputs,
    std::vector<OutputBindingData> const & output_bindings,
    dl::ModelBase const & model,
    int current_frame,
    ImageSize source_image_size) {

    auto const output_slot_vec = model.outputSlots();

    for (auto const & binding : output_bindings) {
        if (binding.data_key.empty()) continue;
        auto it = outputs.find(binding.slot_name);
        if (it == outputs.end()) continue;

        auto const & tensor = it->second;
        auto const * slot = findSlot(output_slot_vec, binding.slot_name);
        if (!slot) continue;

        dl::DecoderParams params;
        params.source_channel = 0;
        params.batch_index = 0;
        params.threshold = binding.threshold;
        params.subpixel = binding.subpixel;
        params.target_image_size = source_image_size;

        if (slot->shape.size() >= 2) {
            params.height =
                static_cast<int>(slot->shape[slot->shape.size() - 2]);
            params.width =
                static_cast<int>(slot->shape[slot->shape.size() - 1]);
        }

        TimeFrameIndex const frame_idx(current_frame);

        if (binding.decoder_id == "TensorToMask2D") {
            dl::TensorToMask2D decoder;
            auto mask = decoder.decode(tensor, params);
            
            // Get or create MaskData and add the decoded mask
            auto mask_data = dm.getData<MaskData>(binding.data_key);
            if (!mask_data) {
                // Create new MaskData if it doesn't exist
                dm.setData<MaskData>(binding.data_key, TimeKey("media"));
                mask_data = dm.getData<MaskData>(binding.data_key);
            }
            if (mask_data && !mask.empty()) {
                mask_data->addAtTime(frame_idx, std::move(mask), NotifyObservers::Yes);
            }

        } else if (binding.decoder_id == "TensorToPoint2D") {
            dl::TensorToPoint2D decoder;
            auto point = decoder.decode(tensor, params);
            
            auto point_data = dm.getData<PointData>(binding.data_key);
            if (!point_data) {
                dm.setData<PointData>(binding.data_key, TimeKey("media"));
                point_data = dm.getData<PointData>(binding.data_key);
            }
            if (point_data) {
                point_data->addAtTime(frame_idx, std::move(point), NotifyObservers::Yes);
            }

        } else if (binding.decoder_id == "TensorToLine2D") {
            dl::TensorToLine2D decoder;
            auto line = decoder.decode(tensor, params);
            
            auto line_data = dm.getData<LineData>(binding.data_key);
            if (!line_data) {
                dm.setData<LineData>(binding.data_key, TimeKey("media"));
                line_data = dm.getData<LineData>(binding.data_key);
            }
            if (line_data && !line.empty()) {
                line_data->addAtTime(frame_idx, std::move(line), NotifyObservers::Yes);
            }

        } else {
            std::cerr << "SlotAssembler: unknown decoder '"
                      << binding.decoder_id << "'\n";
        }
    }
}

} // anonymous namespace

// ════════════════════════════════════════════════════════════════════════════
// Construction / destruction / move
// ════════════════════════════════════════════════════════════════════════════

SlotAssembler::SlotAssembler()
    : _impl(std::make_unique<Impl>()) {}

SlotAssembler::~SlotAssembler() = default;

SlotAssembler::SlotAssembler(SlotAssembler &&) noexcept = default;
SlotAssembler & SlotAssembler::operator=(SlotAssembler &&) noexcept = default;

// ════════════════════════════════════════════════════════════════════════════
// Instance: model lifecycle
// ════════════════════════════════════════════════════════════════════════════

bool SlotAssembler::loadModel(std::string const & model_id) {
    _impl->model.reset();
    _impl->model_id.clear();

    if (model_id.empty()) return false;

    auto model = dl::ModelRegistry::instance().create(model_id);
    if (!model) return false;

    _impl->model = std::move(model);
    _impl->model_id = model_id;
    return true;
}

bool SlotAssembler::loadWeights(std::string const & weights_path) {
    if (!_impl->model) return false;
    if (weights_path.empty()) return false;

    std::filesystem::path const p(weights_path);
    if (!std::filesystem::exists(p)) return false;

    try {
        _impl->model->loadWeights(p);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SlotAssembler::loadWeights: " << e.what() << '\n';
        return false;
    }
}

bool SlotAssembler::isModelReady() const {
    return _impl->model && _impl->model->isReady();
}

std::string const & SlotAssembler::currentModelId() const {
    return _impl->model_id;
}

void SlotAssembler::resetModel() {
    _impl->model.reset();
    _impl->model_id.clear();
}

// ════════════════════════════════════════════════════════════════════════════
// Instance: inference
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::runSingleFrame(
    DataManager & dm,
    std::vector<SlotBindingData> const & input_bindings,
    std::vector<StaticInputData> const & static_inputs,
    std::vector<OutputBindingData> const & output_bindings,
    int current_frame,
    ImageSize source_image_size) {

    if (!isModelReady()) {
        throw std::runtime_error("SlotAssembler::runSingleFrame: "
                                 "model not loaded or weights missing");
    }

    auto inputs = assembleInputs(
        dm, *_impl->model,
        input_bindings, static_inputs,
        current_frame, /*batch_size=*/1);

    auto outputs = _impl->model->forward(inputs);

    decodeOutputs(
        dm, outputs, output_bindings,
        *_impl->model, current_frame, source_image_size);
}

// ════════════════════════════════════════════════════════════════════════════
// Static: registry queries
// ════════════════════════════════════════════════════════════════════════════

std::vector<std::string> SlotAssembler::availableModelIds() {
    return dl::ModelRegistry::instance().availableModels();
}

std::optional<ModelDisplayInfo> SlotAssembler::getModelDisplayInfo(
    std::string const & model_id) {

    auto info = dl::ModelRegistry::instance().getModelInfo(model_id);
    if (!info) return std::nullopt;

    ModelDisplayInfo display;
    display.model_id = info->model_id;
    display.display_name = info->display_name;
    display.description = info->description;
    display.inputs = info->inputs;
    display.outputs = info->outputs;
    display.preferred_batch_size = info->preferred_batch_size;
    display.max_batch_size = info->max_batch_size;
    return display;
}

// ════════════════════════════════════════════════════════════════════════════
// Static: encoder / decoder queries
// ════════════════════════════════════════════════════════════════════════════

std::vector<std::string> SlotAssembler::availableEncoders() {
    return dl::EncoderFactory::availableEncoders();
}

std::vector<std::string> SlotAssembler::availableDecoders() {
    return dl::DecoderFactory::availableDecoders();
}

std::string SlotAssembler::dataTypeForEncoder(std::string const & encoder_id) {
    if (encoder_id == "ImageEncoder") return "MediaData";
    if (encoder_id == "Point2DEncoder") return "PointData";
    if (encoder_id == "Mask2DEncoder") return "MaskData";
    if (encoder_id == "Line2DEncoder") return "LineData";
    return {};
}

std::string SlotAssembler::dataTypeForDecoder(std::string const & decoder_id) {
    if (decoder_id == "TensorToPoint2D") return "PointData";
    if (decoder_id == "TensorToMask2D") return "MaskData";
    if (decoder_id == "TensorToLine2D") return "LineData";
    return {};
}

/// @file DynamicInputSlotWidget.cpp
/// @brief Implementation of DynamicInputSlotWidget.

#include "DynamicInputSlotWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "DeepLearning/channel_encoding/EncoderParamSchemas.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>
#include <variant>

namespace dl::widget {

// ════════════════════════════════════════════════════════════════════════════
// Encoder tag ↔ SlotAssembler encoder ID mapping
// ════════════════════════════════════════════════════════════════════════════

namespace {

/// Map from reflect-cpp variant tag (struct name) to SlotAssembler encoder ID.
constexpr struct {
    char const * tag;
    char const * encoder_id;
} kTagToEncoderId[] = {
        {"ImageEncoderParams", "ImageEncoder"},
        {"Point2DEncoderParams", "Point2DEncoder"},
        {"Mask2DEncoderParams", "Mask2DEncoder"},
        {"Line2DEncoderParams", "Line2DEncoder"},
};

/// Map from SlotAssembler encoder ID to reflect-cpp variant tag.
constexpr struct {
    char const * encoder_id;
    char const * tag;
} kEncoderIdToTag[] = {
        {"ImageEncoder", "ImageEncoderParams"},
        {"Point2DEncoder", "Point2DEncoderParams"},
        {"Mask2DEncoder", "Mask2DEncoderParams"},
        {"Line2DEncoder", "Line2DEncoderParams"},
};

}// namespace

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

DynamicInputSlotWidget::DynamicInputSlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _dm(std::move(dm)),
      _recommended_encoder(slot.recommended_encoder) {

    assert(_dm && "DynamicInputSlotWidget: DataManager must not be null");

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    // Group box with slot name as title
    auto * group = new QGroupBox(QString::fromStdString(slot.name), this);
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    // Shape label
    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    auto * shape_label = new QLabel(tr("Shape: %1").arg(shape_str), group);
    group_layout->addWidget(shape_label);

    // AutoParamWidget powered by DynamicInputSlotParams schema
    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema = WhiskerToolbox::Transforms::V2::extractParameterSchema<DynamicInputSlotParams>();
    _auto_param->setSchema(schema);

    // Set initial encoder from recommended_encoder hint
    if (!_recommended_encoder.empty()) {
        DynamicInputSlotParams initial;
        for (auto const & [encoder_id, tag]: kEncoderIdToTag) {
            if (_recommended_encoder == encoder_id) {
                // Build the matching encoder variant
                if (tag == std::string("ImageEncoderParams")) {
                    initial.encoder = dl::ImageEncoderParams{};
                } else if (tag == std::string("Point2DEncoderParams")) {
                    initial.encoder = dl::Point2DEncoderParams{};
                } else if (tag == std::string("Mask2DEncoderParams")) {
                    initial.encoder = dl::Mask2DEncoderParams{};
                } else if (tag == std::string("Line2DEncoderParams")) {
                    initial.encoder = dl::Line2DEncoderParams{};
                }
                break;
            }
        }
        auto json = rfl::json::write(initial);
        _auto_param->fromJson(json);
    }

    // Populate source combo from DataManager
    _refreshSourceCombo();

    // When parameters change, re-evaluate source→encoder constraints and emit
    connect(_auto_param, &AutoParamWidget::parametersChanged,
            this, [this]() {
                // Get the current encoder tag to figure out compatible sources
                auto current = params();
                auto const encoder_id = _encoderIdFromTag(
                        rfl::json::write(current.encoder));
                if (!encoder_id.empty()) {
                    auto const type_hint = SlotAssembler::dataTypeForEncoder(encoder_id);
                    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
                    auto const keys = getKeysForTypes(*_dm, types);
                    _auto_param->updateAllowedValues("source", keys);
                }
                emit bindingChanged();
            });
}

DynamicInputSlotWidget::~DynamicInputSlotWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

DynamicInputSlotParams DynamicInputSlotWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<DynamicInputSlotParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};// fallback to defaults on parse failure
}

void DynamicInputSlotWidget::setParams(DynamicInputSlotParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
}

void DynamicInputSlotWidget::refreshDataSources() {
    _refreshSourceCombo();
}

std::string const & DynamicInputSlotWidget::slotName() const {
    return _slot_name;
}

SlotBindingData DynamicInputSlotWidget::toSlotBindingData() const {
    auto const p = params();
    SlotBindingData binding;
    binding.slot_name = _slot_name;
    binding.data_key = p.source;
    binding.time_offset = p.time_offset;

    // Visit the encoder variant to extract encoder_id, mode, gaussian_sigma
    p.encoder.visit(
            [&](auto const & enc) {
                using T = std::decay_t<decltype(enc)>;
                if constexpr (std::is_same_v<T, dl::ImageEncoderParams>) {
                    binding.encoder_id = "ImageEncoder";
                    binding.mode = "Raw";
                } else if constexpr (std::is_same_v<T, dl::Point2DEncoderParams>) {
                    binding.encoder_id = "Point2DEncoder";
                    binding.mode = (enc.mode == dl::RasterMode::Heatmap) ? "Heatmap" : "Binary";
                    binding.gaussian_sigma = enc.gaussian_sigma;
                } else if constexpr (std::is_same_v<T, dl::Mask2DEncoderParams>) {
                    binding.encoder_id = "Mask2DEncoder";
                    binding.mode = "Binary";
                } else if constexpr (std::is_same_v<T, dl::Line2DEncoderParams>) {
                    binding.encoder_id = "Line2DEncoder";
                    binding.mode = (enc.mode == dl::RasterMode::Heatmap) ? "Heatmap" : "Binary";
                    binding.gaussian_sigma = enc.gaussian_sigma;
                }
            });

    return binding;
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void DynamicInputSlotWidget::_refreshSourceCombo() {
    if (!_dm) return;

    // Get all keys matching common dynamic input types (media, points, masks, lines)
    auto const all_types = std::vector<DM_DataType>{
            DM_DataType::Video, DM_DataType::Images,
            DM_DataType::Points, DM_DataType::Mask,
            DM_DataType::Line};
    auto const keys = getKeysForTypes(*_dm, all_types);
    _auto_param->updateAllowedValues("source", keys);
}

std::vector<std::string> DynamicInputSlotWidget::_encoderTagsForDataType(
        std::string const & data_type_hint) {
    // Map DataManager type hint → compatible encoder variant tags
    if (data_type_hint == "MediaData") return {"ImageEncoderParams"};
    if (data_type_hint == "PointData") return {"Point2DEncoderParams"};
    if (data_type_hint == "MaskData") return {"Mask2DEncoderParams"};
    if (data_type_hint == "LineData") return {"Line2DEncoderParams"};
    // Unknown → allow all
    return {"ImageEncoderParams", "Point2DEncoderParams",
            "Mask2DEncoderParams", "Line2DEncoderParams"};
}

std::string DynamicInputSlotWidget::_encoderIdFromTag(
        std::string const & json) {
    // Quick parse: find the "encoder" tag value from JSON
    // The JSON looks like: {"encoder":"ImageEncoderParams", ...}
    // We just need the tag, not the full parse
    for (auto const & [tag, encoder_id]: kTagToEncoderId) {
        if (json.find(tag) != std::string::npos) {
            return encoder_id;
        }
    }
    return {};
}

}// namespace dl::widget

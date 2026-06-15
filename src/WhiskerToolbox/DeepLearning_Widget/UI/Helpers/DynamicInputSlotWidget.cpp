/// @file DynamicInputSlotWidget.cpp
/// @brief Implementation of DynamicInputSlotWidget.

#include "DynamicInputSlotWidget.hpp"

#include "DeepLearning_Widget/Core/BindingConversion.hpp"
#include "DeepLearning_Widget/Core/DeepLearningParamSchemasUIHints.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearning/channel_encoding/EncoderDispatch.hpp"
#include "DeepLearning/channel_encoding/EncoderParamSchemas.hpp"
#include "DeepLearning/models_v2/TensorSlotDescriptor.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>
#include <variant>

namespace dl::widget {

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

    auto schema = extractParameterSchema<DynamicInputSlotParams>();
    _auto_param->setSchema(schema);

    // Set initial encoder from recommended_encoder hint
    if (!_recommended_encoder.empty()) {
        DynamicInputSlotParams initial;
        dl::assignEncoderFromFactoryName(initial.encoder, _recommended_encoder);
        auto json = rfl::json::write(initial);
        _auto_param->fromJson(json);
    }

    // Populate source combo from DataManager
    _refreshSourceCombo();

    // When parameters change, re-evaluate source→encoder constraints and emit
    connect(_auto_param, &AutoParamWidget::parametersChanged,
            this, [this]() {
                auto const current = params();
                auto const type_hint =
                        SlotAssembler::dataTypeForEncoder(current.encoder);
                if (!type_hint.empty()) {
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
    return dl::conversion::fromDynamicInputParams(_slot_name, params());
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

}// namespace dl::widget

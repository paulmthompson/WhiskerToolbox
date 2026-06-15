/// @file DynamicInputSlotWidget.cpp
/// @brief Implementation of DynamicInputSlotWidget.

#include "DynamicInputSlotWidget.hpp"

#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/channel_encoding/EncoderDispatch.hpp"
#include "DeepLearning/channel_encoding/EncoderParamSchemas.hpp"
#include "DeepLearning/models_v2/TensorSlotDescriptor.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>

namespace dl::widget {

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

    auto * group = new QGroupBox(QString::fromStdString(slot.name), this);
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    auto * shape_label = new QLabel(tr("Shape: %1").arg(shape_str), group);
    group_layout->addWidget(shape_label);

    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema = extractParameterSchema<DynamicInputBindingForm>();
    _auto_param->setSchema(schema);

    if (!_recommended_encoder.empty()) {
        DynamicInputBindingForm initial;
        dl::assignEncoderFromFactoryName(initial.encoder, _recommended_encoder);
        _setForm(initial);
    }

    _refreshSourceCombo();

    connect(_auto_param, &AutoParamWidget::parametersChanged,
            this, [this]() {
                auto const current = _form();
                auto const type_hint =
                        SlotAssembler::dataTypeForEncoder(current.encoder);
                if (!type_hint.empty()) {
                    auto const types =
                            DataSourceComboHelper::typesFromHint(type_hint);
                    auto const keys = getKeysForTypes(*_dm, types);
                    _auto_param->updateAllowedValues("data_key", keys);
                }
                emit bindingChanged();
            });
}

DynamicInputSlotWidget::~DynamicInputSlotWidget() = default;

SlotBindingData DynamicInputSlotWidget::binding() const {
    return toSlotBindingData(_slot_name, _form());
}

void DynamicInputSlotWidget::setBinding(SlotBindingData const & binding) {
    _setForm(fromSlotBindingData(binding));
    _refreshSourceCombo();
}

void DynamicInputSlotWidget::refreshDataSources() {
    _refreshSourceCombo();
}

std::string const & DynamicInputSlotWidget::slotName() const {
    return _slot_name;
}

DynamicInputBindingForm DynamicInputSlotWidget::_form() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<DynamicInputBindingForm>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void DynamicInputSlotWidget::_setForm(DynamicInputBindingForm const & form) {
    auto json = rfl::json::write(form);
    _auto_param->fromJson(json);
}

void DynamicInputSlotWidget::_refreshSourceCombo() {
    if (!_dm) return;

    auto const all_types = std::vector<DM_DataType>{
            DM_DataType::Video, DM_DataType::Images,
            DM_DataType::Points, DM_DataType::Mask,
            DM_DataType::Line};
    auto const keys = getKeysForTypes(*_dm, all_types);
    _auto_param->updateAllowedValues("data_key", keys);
}

}// namespace dl::widget

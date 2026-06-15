/// @file OutputSlotWidget.cpp
/// @brief Implementation of OutputSlotWidget.

#include "OutputSlotWidget.hpp"

#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning/channel_decoding/DecoderDispatch.hpp"
#include "DeepLearning/channel_decoding/DecoderParamSchemas.hpp"
#include "DeepLearning/models_v2/TensorSlotDescriptor.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>

namespace dl::widget {

OutputSlotWidget::OutputSlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _dm(std::move(dm)),
      _recommended_decoder(slot.recommended_decoder) {

    assert(_dm && "OutputSlotWidget: DataManager must not be null");

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

    auto schema = extractParameterSchema<OutputBindingForm>();
    _auto_param->setSchema(schema);

    if (!_recommended_decoder.empty()) {
        OutputBindingForm initial;
        if (auto const params =
                    dl::decoderParamsFromFactoryName(_recommended_decoder)) {
            initial.decoder = *params;
        }
        _setForm(initial);
    }

    _refreshTargetCombo();

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        auto const current = _form();
        auto const type_hint = SlotAssembler::dataTypeForDecoder(current.decoder);
        if (!type_hint.empty()) {
            auto const types =
                    DataSourceComboHelper::typesFromHint(type_hint);
            auto const keys = getKeysForTypes(*_dm, types);
            _auto_param->updateAllowedValues("data_key", keys);
        }
        emit bindingChanged();
    });
}

OutputSlotWidget::~OutputSlotWidget() = default;

OutputBindingData OutputSlotWidget::binding() const {
    return toOutputBindingData(_slot_name, _form());
}

void OutputSlotWidget::setBinding(OutputBindingData const & binding) {
    _setForm(fromOutputBindingData(binding));
    _refreshTargetCombo();
}

void OutputSlotWidget::refreshDataSources() {
    _refreshTargetCombo();
}

void OutputSlotWidget::updateDecoderAlternatives(
        std::vector<std::string> const & allowed_tags) {
    _auto_param->updateVariantAlternatives("decoder", allowed_tags);
}

std::string const & OutputSlotWidget::slotName() const {
    return _slot_name;
}

OutputBindingForm OutputSlotWidget::_form() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<OutputBindingForm>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void OutputSlotWidget::_setForm(OutputBindingForm const & form) {
    auto json = rfl::json::write(form);
    _auto_param->fromJson(json);
}

void OutputSlotWidget::_refreshTargetCombo() {
    if (!_dm) return;

    auto const current = _form();
    std::string type_hint = SlotAssembler::dataTypeForDecoder(current.decoder);
    if (type_hint.empty() && !_recommended_decoder.empty()) {
        type_hint = SlotAssembler::dataTypeForDecoder(_recommended_decoder);
    }
    if (type_hint.empty()) {
        type_hint = "MaskData";
    }
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);
    _auto_param->updateAllowedValues("data_key", keys);
}

}// namespace dl::widget

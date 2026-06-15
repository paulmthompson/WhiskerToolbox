/// @file EncoderShapeWidget.cpp
/// @brief Implementation of EncoderShapeWidget.

#include "EncoderShapeWidget.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DeepLearning/registry/ModelRegistry.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <cassert>

namespace dl::widget {

EncoderShapeWidget::EncoderShapeWidget(
        std::shared_ptr<DeepLearningState> state,
        SlotAssembler * assembler,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _assembler(assembler) {

    assert(_state && "EncoderShapeWidget: state must not be null");

    auto const model_id = _state->selectedModelId();
    auto const schema =
            dl::ModelRegistry::instance().getConfigurationSchema(model_id);
    assert(schema.has_value() && "EncoderShapeWidget: model has no configuration schema");

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto * group = new QGroupBox(tr("Model Configuration"), this);
    group->setObjectName(QStringLiteral("model_configuration_group"));
    group->setToolTip(
            tr("Configure model-specific parameters before loading weights."));
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);
    _auto_param->setSchema(*schema);

    auto const stored_json = _state->configurationJsonForModel(model_id);
    _auto_param->fromJson(
            dl::ModelRegistry::instance().configurationFormJson(
                    model_id, stored_json));

    _apply_btn = new QPushButton(tr("Apply Configuration"), group);
    _apply_btn->setToolTip(
            tr("Apply the configured parameters to the model.\n"
               "You must re-load weights after changing configuration."));
    group_layout->addWidget(_apply_btn);

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        _syncToState();
        emit bindingChanged();
    });
    connect(_apply_btn, &QPushButton::clicked, this,
            &EncoderShapeWidget::_onApplyClicked);
}

EncoderShapeWidget::~EncoderShapeWidget() = default;

void EncoderShapeWidget::applyConfigurationFromState(
        DeepLearningState const * state,
        SlotAssembler * assembler) {
    assert(state != nullptr);
    if (assembler == nullptr || state->selectedModelId().empty()) {
        return;
    }
    assembler->applyModelConfiguration(state->activeConfigurationJson());
}

void EncoderShapeWidget::applyFromState() {
    applyConfigurationFromState(_state.get(), _assembler);
}

void EncoderShapeWidget::_syncToState() {
    auto const model_id = _state->selectedModelId();
    auto const form_json = _auto_param->toJson();
    _state->setConfigurationJsonForModel(
            model_id,
            dl::ModelRegistry::instance().configurationStoredJson(
                    model_id, form_json, false));
}

void EncoderShapeWidget::_onApplyClicked() {
    auto const model_id = _state->selectedModelId();
    auto const form_json = _auto_param->toJson();
    _state->setConfigurationJsonForModel(
            model_id,
            dl::ModelRegistry::instance().configurationStoredJson(
                    model_id, form_json, true));
    applyConfigurationFromState(_state.get(), _assembler);
    emit configurationApplied();
}

}// namespace dl::widget

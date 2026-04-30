/// @file EncoderShapeWidget.cpp
/// @brief Implementation of EncoderShapeWidget.

#include "EncoderShapeWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DeepLearning_Widget/Core/DeepLearningParamSchemasUIHints.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>
#include <sstream>

namespace dl::widget {

namespace {

/// Parse comma-separated output shape string into vector of int64_t.
[[nodiscard]] std::vector<int64_t> parseOutputShape(std::string const & shape_str) {
    std::vector<int64_t> out_shape;
    if (shape_str.empty()) return out_shape;

    std::istringstream iss(shape_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try {
            auto val = std::stoll(token);
            if (val > 0) {
                out_shape.push_back(val);
            }
        } catch (std::exception const &) {
            // Non-numeric token — skip it
        }
    }
    return out_shape;
}

}// namespace

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

EncoderShapeWidget::EncoderShapeWidget(
        std::shared_ptr<DeepLearningState> state,
        SlotAssembler * assembler,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _assembler(assembler) {

    assert(_state && "EncoderShapeWidget: state must not be null");

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto * group = new QGroupBox(tr("Encoder Shape"), this);
    group->setObjectName(QStringLiteral("encoder_shape_group"));
    group->setToolTip(
            tr("Configure the input resolution and output shape to match your\n"
               "compiled model. The defaults (224×224 input, 384×7×7 output)\n"
               "only apply to the default ConvNeXt-Tiny backbone.\n\n"
               "Set these BEFORE loading weights."));
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema =
            extractParameterSchema<
                    EncoderShapeParams>();
    _auto_param->setSchema(schema);

    // Restore from saved state
    setParams(paramsFromState(
            _state->encoderInputHeight(),
            _state->encoderInputWidth(),
            _state->encoderOutputShape()));

    _apply_btn = new QPushButton(tr("Apply Shape"), group);
    _apply_btn->setToolTip(
            tr("Apply the configured shape to the model.\n"
               "You must re-load weights after changing the shape."));
    group_layout->addWidget(_apply_btn);

    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        _syncToState();
        emit bindingChanged();
    });
    connect(_apply_btn, &QPushButton::clicked, this,
            &EncoderShapeWidget::_onApplyClicked);
}

EncoderShapeWidget::~EncoderShapeWidget() = default;

void EncoderShapeWidget::applyEncoderShapeToAssembler(
        DeepLearningState const * state,
        SlotAssembler * assembler) {
    assert(state != nullptr);

    int const h = state->encoderInputHeight();
    int const w = state->encoderInputWidth();
    int const effective_h = (h > 0) ? h : 224;
    int const effective_w = (w > 0) ? w : 224;

    auto const out_shape =
            parseOutputShape(state->encoderOutputShape());

    if (assembler) {
        assembler->configureModelShape(effective_h, effective_w, out_shape);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

EncoderShapeParams EncoderShapeWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<EncoderShapeParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};
}

void EncoderShapeWidget::setParams(EncoderShapeParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
}

void EncoderShapeWidget::applyFromState() {
    applyEncoderShapeToAssembler(_state.get(), _assembler);
}

EncoderShapeParams EncoderShapeWidget::paramsFromState(
        int input_height,
        int input_width,
        std::string const & output_shape) {
    EncoderShapeParams p;
    p.input_height = (input_height > 0) ? input_height : 224;
    p.input_width = (input_width > 0) ? input_width : 224;
    p.output_shape = output_shape;
    return p;
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void EncoderShapeWidget::_syncToState() {
    auto const p = params();
    _state->setEncoderInputHeight(p.input_height.value());
    _state->setEncoderInputWidth(p.input_width.value());
    _state->setEncoderOutputShape(p.output_shape);
}

void EncoderShapeWidget::_onApplyClicked() {
    _syncToState();
    applyEncoderShapeToAssembler(_state.get(), _assembler);
    emit shapeApplied();
}

}// namespace dl::widget

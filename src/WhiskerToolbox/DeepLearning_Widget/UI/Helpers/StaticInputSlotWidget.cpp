/// @file StaticInputSlotWidget.cpp
/// @brief Implementation of StaticInputSlotWidget.

#include "StaticInputSlotWidget.hpp"

#include "DeepLearning_Widget/Core/BindingConversion.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <cassert>
#include <type_traits>

namespace dl::widget {

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

StaticInputSlotWidget::StaticInputSlotWidget(
        dl::TensorSlotDescriptor const & slot,
        std::shared_ptr<DataManager> dm,
        QWidget * parent)
    : QWidget(parent),
      _slot_name(slot.name),
      _recommended_encoder(slot.recommended_encoder),
      _dm(std::move(dm)) {

    assert(_dm && "StaticInputSlotWidget: DataManager must not be null");

    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    // Group box with slot name + (memory) suffix as title
    auto * group = new QGroupBox(
            QString::fromStdString(slot.name) + tr(" (memory)"), this);
    main_layout->addWidget(group);

    auto * group_layout = new QVBoxLayout(group);

    if (!slot.description.empty()) {
        group->setToolTip(QString::fromStdString(slot.description));
    }

    // Descriptive info label
    auto * info = new QLabel(
            tr("Relative: re-encodes each run at current_frame + offset.\n"
               "Absolute: capture once at a chosen frame and reuse."),
            group);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    group_layout->addWidget(info);

    // Shape label
    QString shape_str;
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (i > 0) shape_str += QStringLiteral(" \u00D7 ");
        shape_str += QString::number(slot.shape[i]);
    }
    auto * shape_label = new QLabel(tr("Shape: %1").arg(shape_str), group);
    group_layout->addWidget(shape_label);

    // AutoParamWidget powered by StaticInputSlotParams schema
    _auto_param = new AutoParamWidget(group);
    group_layout->addWidget(_auto_param);

    auto schema =
            extractParameterSchema<StaticInputSlotParams>();
    _auto_param->setSchema(schema);

    // Capture row — shown only in Absolute mode
    _capture_row_widget = new QWidget(group);
    auto * capture_row_layout = new QHBoxLayout(_capture_row_widget);
    capture_row_layout->setContentsMargins(0, 0, 0, 0);

    _capture_btn = new QPushButton(tr("\u2B07 Capture Current Frame"),
                                   _capture_row_widget);
    _capture_btn->setToolTip(
            tr("Encode data from the current frame and freeze it.\n"
               "The cached tensor will be reused for all subsequent runs."));
    _capture_btn->setEnabled(false);
    capture_row_layout->addWidget(_capture_btn);
    capture_row_layout->addStretch();

    _capture_status = new QLabel(tr("Not captured"), _capture_row_widget);
    _capture_status->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    capture_row_layout->addWidget(_capture_status);

    _capture_row_widget->setVisible(false);
    group_layout->addWidget(_capture_row_widget);

    // Populate source combo from DataManager
    _refreshSourceCombo();

    // Wire capture button
    connect(_capture_btn, &QPushButton::clicked,
            this, [this]() { emit captureRequested(_slot_name); });

    // When parameters change: handle source changes, mode toggling, and notify
    connect(_auto_param, &AutoParamWidget::parametersChanged, this, [this]() {
        auto current = params();

        // Detect source changes and invalidate cache
        if (current.source != _last_source) {
            _last_source = current.source;
            _captured_frame = -1;
            _capture_status->setText(tr("Not captured"));
            _capture_status->setStyleSheet(
                    QStringLiteral("color: gray; font-size: 10px;"));
            emit captureInvalidated(_slot_name);
        }

        // Toggle capture row visibility based on active capture mode
        bool const is_absolute = _isAbsoluteMode();
        _capture_row_widget->setVisible(is_absolute);
        _updateCaptureButtonEnabled();

        emit bindingChanged();
    });
}

StaticInputSlotWidget::~StaticInputSlotWidget() = default;

// ════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════

StaticInputSlotParams StaticInputSlotWidget::params() const {
    auto json_str = _auto_param->toJson();
    auto result = rfl::json::read<StaticInputSlotParams>(json_str);
    if (result) {
        return result.value();
    }
    return {};// fallback to defaults on parse failure
}

void StaticInputSlotWidget::setParams(StaticInputSlotParams const & p) {
    auto json = rfl::json::write(p);
    _auto_param->fromJson(json);
}

void StaticInputSlotWidget::refreshDataSources() {
    _refreshSourceCombo();
}

std::string const & StaticInputSlotWidget::slotName() const {
    return _slot_name;
}

int StaticInputSlotWidget::capturedFrame() const {
    return _captured_frame;
}

StaticInputData StaticInputSlotWidget::toStaticInputData() const {
    return dl::conversion::fromStaticInputParams(
            _slot_name, params(), _captured_frame);
}

void StaticInputSlotWidget::setCapturedStatus(
        int captured_frame,
        std::pair<float, float> value_range) {
    _captured_frame = captured_frame;

    QString info = tr("\u2713 Captured");
    if (captured_frame >= 0) {
        info += tr(" (frame %1)").arg(captured_frame);
    }
    info += tr(" range [%1, %2]")
                    .arg(static_cast<double>(value_range.first), 0, 'f', 2)
                    .arg(static_cast<double>(value_range.second), 0, 'f', 2);
    _capture_status->setText(info);
    _capture_status->setStyleSheet(QStringLiteral("color: green; font-size: 10px;"));
}

void StaticInputSlotWidget::clearCapturedStatus() {
    _captured_frame = -1;
    _capture_status->setText(tr("Not captured"));
    _capture_status->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
}

void StaticInputSlotWidget::setModelReady(bool ready) {
    _model_ready = ready;
    _updateCaptureButtonEnabled();
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void StaticInputSlotWidget::_refreshSourceCombo() {
    if (!_dm) return;

    auto const type_hint = SlotAssembler::dataTypeForEncoder(_recommended_encoder);
    auto const types = DataSourceComboHelper::typesFromHint(type_hint);
    auto const keys = getKeysForTypes(*_dm, types);
    _auto_param->updateAllowedValues("source", keys);
}

void StaticInputSlotWidget::_updateCaptureButtonEnabled() {
    if (_capture_btn) {
        _capture_btn->setEnabled(_isAbsoluteMode() && _model_ready);
    }
}

bool StaticInputSlotWidget::_isAbsoluteMode() const {
    auto const p = params();
    bool is_absolute = false;
    p.capture_mode.visit([&](auto const & cm) {
        using T = std::decay_t<decltype(cm)>;
        if constexpr (std::is_same_v<T, AbsoluteCaptureParams>) {
            is_absolute = true;
        }
    });
    return is_absolute;
}

}// namespace dl::widget

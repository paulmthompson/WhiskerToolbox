/**
 * @file CanvasCoord_Widget.cpp
 * @brief Implementation of the canvas coordinate system display/override widget
 */

#include "CanvasCoord_Widget.hpp"

#include "Media_Widget/Core/MediaWidgetState.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

CanvasCoord_Widget::CanvasCoord_Widget(QWidget * parent)
    : QWidget(parent) {
    _buildUi();
}

void CanvasCoord_Widget::setState(MediaWidgetState * state) {
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;
    if (!_state) return;

    connect(_state, &MediaWidgetState::canvasCoordinateSystemChanged,
            this, &CanvasCoord_Widget::_onCoordSystemChanged);

    // Sync initial state
    _updateDisplay(_state->canvasCoordinateSystem());

    bool const override_active = _state->isCanvasCoordOverrideActive();
    _override_checkbox->setChecked(override_active);
    _width_spin->setEnabled(override_active);
    _height_spin->setEnabled(override_active);

    if (override_active) {
        auto const & d = _state->data();
        _width_spin->setValue(d.canvas_coord_override_width);
        _height_spin->setValue(d.canvas_coord_override_height);
    }
}

void CanvasCoord_Widget::_buildUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    // Resolved info
    _resolved_label = new QLabel(QStringLiteral("640 × 480"), this);
    _source_label = new QLabel(QStringLiteral("Source: Default"), this);
    _source_label->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));

    auto * info_layout = new QFormLayout();
    info_layout->setContentsMargins(0, 0, 0, 0);
    info_layout->addRow(QStringLiteral("Resolution:"), _resolved_label);
    info_layout->addRow(QStringLiteral(""), _source_label);
    layout->addLayout(info_layout);

    // Override controls
    _override_checkbox = new QCheckBox(QStringLiteral("Manual override"), this);
    layout->addWidget(_override_checkbox);

    auto * dims_layout = new QHBoxLayout();
    dims_layout->setContentsMargins(16, 0, 0, 0);

    _width_spin = new QSpinBox(this);
    _width_spin->setRange(1, 16384);
    _width_spin->setValue(640);
    _width_spin->setSuffix(QStringLiteral(" px"));
    _width_spin->setEnabled(false);

    _height_spin = new QSpinBox(this);
    _height_spin->setRange(1, 16384);
    _height_spin->setValue(480);
    _height_spin->setSuffix(QStringLiteral(" px"));
    _height_spin->setEnabled(false);

    dims_layout->addWidget(new QLabel(QStringLiteral("W:"), this));
    dims_layout->addWidget(_width_spin);
    dims_layout->addWidget(new QLabel(QStringLiteral("H:"), this));
    dims_layout->addWidget(_height_spin);
    layout->addLayout(dims_layout);

    // Connections
    connect(_override_checkbox, &QCheckBox::toggled,
            this, &CanvasCoord_Widget::_onOverrideToggled);
    connect(_width_spin, &QSpinBox::editingFinished,
            this, &CanvasCoord_Widget::_onOverrideDimsChanged);
    connect(_height_spin, &QSpinBox::editingFinished,
            this, &CanvasCoord_Widget::_onOverrideDimsChanged);
}

void CanvasCoord_Widget::_onCoordSystemChanged(CanvasCoordinateSystem const & coord_system) {
    _updateDisplay(coord_system);
}

void CanvasCoord_Widget::_onOverrideToggled(bool checked) {
    _width_spin->setEnabled(checked);
    _height_spin->setEnabled(checked);

    if (!_state) return;

    if (checked) {
        _state->setCanvasCoordOverride(_width_spin->value(), _height_spin->value());
    } else {
        _state->clearCanvasCoordOverride();
    }
}

void CanvasCoord_Widget::_onOverrideDimsChanged() {
    if (!_state || !_override_checkbox->isChecked()) return;
    _state->setCanvasCoordOverride(_width_spin->value(), _height_spin->value());
}

void CanvasCoord_Widget::_updateDisplay(CanvasCoordinateSystem const & cs) {
    _resolved_label->setText(
            QString::number(cs.width) + QStringLiteral(" × ") + QString::number(cs.height));
    _source_label->setText(_sourceToString(cs.source, cs.source_key));

    // When not overriding, keep spinboxes in sync with resolved values
    if (!_override_checkbox->isChecked()) {
        QSignalBlocker const wb(_width_spin);
        QSignalBlocker const hb(_height_spin);
        _width_spin->setValue(cs.width);
        _height_spin->setValue(cs.height);
    }
}

QString CanvasCoord_Widget::_sourceToString(CanvasCoordinateSource source,
                                            std::string const & key) {
    switch (source) {
        case CanvasCoordinateSource::Default:
            return QStringLiteral("Source: Default (640×480)");
        case CanvasCoordinateSource::Media:
            return QStringLiteral("Source: Media \"") +
                   QString::fromStdString(key) + QStringLiteral("\"");
        case CanvasCoordinateSource::DataObject:
            return QStringLiteral("Source: Data \"") +
                   QString::fromStdString(key) + QStringLiteral("\"");
        case CanvasCoordinateSource::UserOverride:
            return QStringLiteral("Source: Manual override");
    }
    return QStringLiteral("Source: Unknown");
}

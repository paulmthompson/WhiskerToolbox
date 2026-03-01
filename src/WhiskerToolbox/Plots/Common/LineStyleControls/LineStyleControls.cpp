#include "LineStyleControls.hpp"
#include "Core/LineStyleState.hpp"

#include "CorePlotting/DataTypes/LineStyleData.hpp"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>

#include <cmath>

LineStyleControls::LineStyleControls(
    LineStyleState * state,
    QWidget * parent)
    : QWidget(parent),
      _state(state),
      _color_button(nullptr),
      _thickness_spinbox(nullptr),
      _alpha_spinbox(nullptr),
      _updating_ui(false)
{
    auto * layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Color button
    _color_button = new QPushButton(this);
    _color_button->setFixedSize(60, 24);
    _color_button->setCursor(Qt::PointingHandCursor);
    layout->addRow("Color:", _color_button);

    // Thickness spin box
    _thickness_spinbox = new QDoubleSpinBox(this);
    _thickness_spinbox->setMinimum(0.5);
    _thickness_spinbox->setMaximum(20.0);
    _thickness_spinbox->setSingleStep(0.5);
    _thickness_spinbox->setDecimals(1);
    _thickness_spinbox->setSuffix(" px");
    layout->addRow("Thickness:", _thickness_spinbox);

    // Alpha spin box
    _alpha_spinbox = new QDoubleSpinBox(this);
    _alpha_spinbox->setMinimum(0.0);
    _alpha_spinbox->setMaximum(1.0);
    _alpha_spinbox->setSingleStep(0.05);
    _alpha_spinbox->setDecimals(2);
    layout->addRow("Alpha:", _alpha_spinbox);

    // Connect UI signals
    connect(_color_button, &QPushButton::clicked,
            this, &LineStyleControls::onColorClicked);
    connect(_thickness_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineStyleControls::onThicknessChanged);
    connect(_alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineStyleControls::onAlphaChanged);

    // Connect to state signals
    if (_state) {
        connect(_state, &LineStyleState::styleChanged,
                this, &LineStyleControls::onStateStyleChanged);
        connect(_state, &LineStyleState::styleUpdated,
                this, &LineStyleControls::onStateStyleUpdated);
    }

    // Initialize UI from state
    updateFromState();
}

void LineStyleControls::onColorClicked()
{
    if (!_state) return;

    QColor const current = QColor(QString::fromStdString(_state->hexColor()));
    QColor const chosen = QColorDialog::getColor(current, this, "Select Line Color");

    if (chosen.isValid()) {
        _state->setHexColor(chosen.name().toStdString());
    }
}

void LineStyleControls::onThicknessChanged(double value)
{
    if (_updating_ui || !_state) return;

    _state->setThickness(static_cast<float>(value));
}

void LineStyleControls::onAlphaChanged(double value)
{
    if (_updating_ui || !_state) return;

    _state->setAlpha(static_cast<float>(value));
}

void LineStyleControls::onStateStyleChanged()
{
    updateFromState();
}

void LineStyleControls::onStateStyleUpdated()
{
    updateFromState();
}

void LineStyleControls::updateFromState()
{
    if (!_state) {
        // Clear/reset UI to defaults when no state is bound
        _updating_ui = true;
        _thickness_spinbox->setValue(1.0);
        _alpha_spinbox->setValue(1.0);
        _color_button->setStyleSheet("background-color: #007bff; border: 1px solid #888;");
        _updating_ui = false;
        return;
    }

    _updating_ui = true;

    auto const & data = _state->data();

    // Update thickness
    if (std::abs(_thickness_spinbox->value() - static_cast<double>(data.thickness)) > 0.01) {
        _thickness_spinbox->setValue(static_cast<double>(data.thickness));
    }

    // Update alpha
    if (std::abs(_alpha_spinbox->value() - static_cast<double>(data.alpha)) > 0.01) {
        _alpha_spinbox->setValue(static_cast<double>(data.alpha));
    }

    // Update color button
    updateColorButtonStyle();

    _updating_ui = false;
}

void LineStyleControls::updateColorButtonStyle()
{
    if (!_state) return;

    QString const hex = QString::fromStdString(_state->hexColor());
    _color_button->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #888;").arg(hex));
    _color_button->setToolTip(hex);
}

void LineStyleControls::setLineStyleState(LineStyleState * state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        connect(_state, &LineStyleState::styleChanged,
                this, &LineStyleControls::onStateStyleChanged);
        connect(_state, &LineStyleState::styleUpdated,
                this, &LineStyleControls::onStateStyleUpdated);
    }
    setEnabled(_state != nullptr);
    updateFromState();
}

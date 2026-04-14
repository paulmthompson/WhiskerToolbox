/**
 * @file ColorAlphaControls.cpp
 * @brief Implementation of the compact color + alpha editing widget
 */

#include "ColorAlphaControls.hpp"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>

ColorAlphaControls::ColorAlphaControls(QWidget * parent)
    : QWidget(parent),
      _color_button(nullptr),
      _alpha_spinbox(nullptr) {
    auto * layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Color button — shows current color, opens QColorDialog on click
    _color_button = new QPushButton(this);
    _color_button->setFixedSize(60, 24);
    _color_button->setCursor(Qt::PointingHandCursor);
    layout->addRow("Color:", _color_button);

    // Alpha spin box
    _alpha_spinbox = new QDoubleSpinBox(this);
    _alpha_spinbox->setMinimum(0.0);
    _alpha_spinbox->setMaximum(1.0);
    _alpha_spinbox->setSingleStep(0.05);
    _alpha_spinbox->setDecimals(2);
    layout->addRow("Alpha:", _alpha_spinbox);

    // Connect UI signals
    connect(_color_button, &QPushButton::clicked,
            this, &ColorAlphaControls::_onColorClicked);
    connect(_alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorAlphaControls::_onAlphaChanged);

    // Initialize display
    _alpha_spinbox->setValue(1.0);
    _updateColorButtonStyle();
}

void ColorAlphaControls::setColor(QString const & hex_color) {
    _hex_color = hex_color;
    _updateColorButtonStyle();
}

void ColorAlphaControls::setAlpha(float alpha) {
    _alpha_spinbox->blockSignals(true);
    _alpha_spinbox->setValue(static_cast<double>(alpha));
    _alpha_spinbox->blockSignals(false);
}

QString ColorAlphaControls::hexColor() const {
    return _hex_color;
}

float ColorAlphaControls::alpha() const {
    return static_cast<float>(_alpha_spinbox->value());
}

void ColorAlphaControls::_onColorClicked() {
    QColor const current(_hex_color);
    QColor const chosen = QColorDialog::getColor(current, this, "Select Color");

    if (chosen.isValid()) {
        _hex_color = chosen.name();
        _updateColorButtonStyle();
        emit colorChanged(_hex_color);
    }
}

void ColorAlphaControls::_onAlphaChanged(double value) {
    emit alphaChanged(static_cast<float>(value));
}

void ColorAlphaControls::_updateColorButtonStyle() {
    _color_button->setStyleSheet(
            QStringLiteral("background-color: %1; border: 1px solid #888;").arg(_hex_color));
    _color_button->setToolTip(_hex_color);
}

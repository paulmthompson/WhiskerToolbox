
#include "ColorPicker_Widget.hpp"
#include "ui_ColorPicker_Widget.h"

#include "utils/color.hpp"
#include <QColorDialog>
#include <iostream>

ColorPicker_Widget::ColorPicker_Widget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ColorPicker_Widget) {
    ui->setupUi(this);

    connect(ui->red_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ColorPicker_Widget::_onRgbChanged);
    connect(ui->green_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ColorPicker_Widget::_onRgbChanged);
    connect(ui->blue_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ColorPicker_Widget::_onRgbChanged);

    connect(ui->lineEdit, &QLineEdit::textChanged,
            this, &ColorPicker_Widget::_onHexChanged);

    connect(ui->horizontalSlider, &QSlider::valueChanged,
            this, &ColorPicker_Widget::_onAlphaChanged);

    // Default color (blue)
    setColor("#0000FF");
}

ColorPicker_Widget::~ColorPicker_Widget() {
    delete ui;
}

void ColorPicker_Widget::setColor(const QString& hex_color) {
    if (_updating) return;

    _updating = true;

    // Set the hex text field
    ui->lineEdit->setText(hex_color);

    // Convert hex to RGB
    QColor color(hex_color);
    ui->red_spinbox->setValue(color.red());
    ui->green_spinbox->setValue(color.green());
    ui->blue_spinbox->setValue(color.blue());

    _updateColorDisplay();

    _updating = false;
}

void ColorPicker_Widget::setColor(int r, int g, int b) {
    if (_updating) return;

    _updating = true;

    // Set RGB values
    ui->red_spinbox->setValue(r);
    ui->green_spinbox->setValue(g);
    ui->blue_spinbox->setValue(b);

    // Convert to hex and set hex field
    QColor color(r, g, b);
    ui->lineEdit->setText(color.name());

    _updateColorDisplay();

    _updating = false;
}

void ColorPicker_Widget::setAlpha(int alpha_percent) {
    ui->horizontalSlider->setValue(alpha_percent);
    // _onAlphaChanged will be triggered by the slider's signal
}

QString ColorPicker_Widget::getHexColor() const {
    return ui->lineEdit->text();
}

QColor ColorPicker_Widget::getColor() const {
    return QColor(ui->red_spinbox->value(),
                  ui->green_spinbox->value(),
                  ui->blue_spinbox->value());
}

int ColorPicker_Widget::getAlphaPercent() const {
    return ui->horizontalSlider->value();
}

float ColorPicker_Widget::getAlphaFloat() const {
    return static_cast<float>(ui->horizontalSlider->value()) / 100.0f;
}

void ColorPicker_Widget::_onRgbChanged() {
    if (_updating) return;

    _updating = true;

    // Get RGB values
    int r = ui->red_spinbox->value();
    int g = ui->green_spinbox->value();
    int b = ui->blue_spinbox->value();

    // Convert to hex and update hex field
    QColor color(r, g, b);
    ui->lineEdit->setText(color.name());

    _updateColorDisplay();

    // Emit signal
    emit colorChanged(color.name());
    emit colorAndAlphaChanged(color.name(), getAlphaFloat());

    _updating = false;
}

void ColorPicker_Widget::_onHexChanged() {
    if (_updating) return;

    _updating = true;

    QString hexColor = ui->lineEdit->text();
    if (isValidHexColor(hexColor.toStdString())) {
        QColor color(hexColor);

        // Update RGB spinboxes
        ui->red_spinbox->setValue(color.red());
        ui->green_spinbox->setValue(color.green());
        ui->blue_spinbox->setValue(color.blue());

        _updateColorDisplay();

        // Emit signal
        emit colorChanged(hexColor);
        emit colorAndAlphaChanged(hexColor, getAlphaFloat());
    }

    _updating = false;
}

void ColorPicker_Widget::_onAlphaChanged(int value) {
    _updateColorDisplay();

    // Emit signals
    emit alphaChanged(value);
    emit colorAndAlphaChanged(getHexColor(), getAlphaFloat());
}

void ColorPicker_Widget::_updateColorDisplay() {
    // Update the color preview widget
    QColor color = getColor();
    color.setAlpha(static_cast<int>(getAlphaFloat() * 255));

    QString styleSheet = QString("background-color: %1;").arg(color.name(color.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb));
    ui->color_preview->setStyleSheet(styleSheet);
}
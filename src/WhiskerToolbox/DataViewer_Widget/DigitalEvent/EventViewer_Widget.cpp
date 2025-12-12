#include "EventViewer_Widget.hpp"
#include "ui_EventViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <QColorDialog>
#include <iostream>

EventViewer_Widget::EventViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget} {
    ui->setupUi(this);

    // Set the color display button to be flat and show just the color
    ui->color_display_button->setFlat(false);
    ui->color_display_button->setEnabled(false); // Make it non-clickable, just for display

    connect(ui->color_button, &QPushButton::clicked,
            this, &EventViewer_Widget::_openColorDialog);

    // Connect display option controls
    connect(ui->mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventViewer_Widget::_setDisplayMode);
    connect(ui->spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventViewer_Widget::_setVerticalSpacing);
    connect(ui->height_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventViewer_Widget::_setEventHeight);
}

EventViewer_Widget::~EventViewer_Widget() {
    delete ui;
}

void EventViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color and display options to current values from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(key);
        if (config.has_value()) {
            _updateColorDisplay(QString::fromStdString(config.value()->style.hex_color));

            // Set display mode controls
            ui->mode_combo->setCurrentIndex(static_cast<int>(config.value()->display_mode));
            ui->spacing_spinbox->setValue(static_cast<double>(config.value()->vertical_spacing));
            ui->height_spinbox->setValue(static_cast<double>(config.value()->event_height));
        } else {
            _updateColorDisplay("#FF0000");// Default red
            ui->mode_combo->setCurrentIndex(0);   // Default to Stacked
            ui->spacing_spinbox->setValue(0.1);   // Default spacing
            ui->height_spinbox->setValue(0.05);   // Default height
        }
    }

    std::cout << "EventViewer_Widget: Active key set to " << key << std::endl;
}

void EventViewer_Widget::_openColorDialog() {
    if (_active_key.empty()) {
        return;
    }
    
    // Get current color
    QColor currentColor;
    auto config = _opengl_widget->getDigitalEventConfig(_active_key);
    if (config.has_value()) {
        currentColor = QColor(QString::fromStdString(config.value()->style.hex_color));
    } else {
        currentColor = QColor("#FF0000");
    }
    
    // Open color dialog
    QColor color = QColorDialog::getColor(currentColor, this, "Choose Color");
    
    if (color.isValid()) {
        QString hex_color = color.name();
        _updateColorDisplay(hex_color);
        _setEventColor(hex_color);
    }
}

void EventViewer_Widget::_updateColorDisplay(QString const & hex_color) {
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
        QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color)
    );
}

void EventViewer_Widget::_setEventColor(QString const & hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->style.hex_color = hex_color.toStdString();
            emit colorChanged(_active_key, hex_color.toStdString());
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setEventAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->style.alpha = alpha_float;
            emit alphaChanged(_active_key, alpha_float);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setDisplayMode(int mode_index) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->display_mode = static_cast<EventDisplayMode>(mode_index);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setVerticalSpacing(double spacing) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->vertical_spacing = static_cast<float>(spacing);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setEventHeight(double height) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->event_height = static_cast<float>(height);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

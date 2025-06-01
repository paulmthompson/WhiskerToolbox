#include "EventViewer_Widget.hpp"
#include "ui_EventViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"
#include "DataViewer_Widget/DisplayOptions/TimeSeriesDisplayOptions.hpp"

#include <iostream>

EventViewer_Widget::EventViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
    
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &EventViewer_Widget::_setEventColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &EventViewer_Widget::_setEventAlpha);
    
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
    
    // Set the color picker and display options to current values from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(key);
        if (config.has_value()) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
            
            // Set display mode controls
            ui->mode_combo->setCurrentIndex(static_cast<int>(config.value()->display_mode));
            ui->spacing_spinbox->setValue(static_cast<double>(config.value()->vertical_spacing));
            ui->height_spinbox->setValue(static_cast<double>(config.value()->event_height));
        } else {
            ui->color_picker->setColor("#FF0000"); // Default red
            ui->color_picker->setAlpha(100); // Default to full opacity
            ui->mode_combo->setCurrentIndex(0); // Default to Stacked
            ui->spacing_spinbox->setValue(0.1); // Default spacing
            ui->height_spinbox->setValue(0.05); // Default height
        }
    }
    
    std::cout << "EventViewer_Widget: Active key set to " << key << std::endl;
}

void EventViewer_Widget::_setEventColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color.toStdString();
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
        emit colorChanged(_active_key, hex_color.toStdString());
    }
}

void EventViewer_Widget::_setEventAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->alpha = alpha_float;
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
        emit alphaChanged(_active_key, alpha_float);
    }
}

void EventViewer_Widget::_setDisplayMode(int mode_index) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->display_mode = static_cast<EventDisplayMode>(mode_index);
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
}

void EventViewer_Widget::_setVerticalSpacing(double spacing) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->vertical_spacing = static_cast<float>(spacing);
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
}

void EventViewer_Widget::_setEventHeight(double height) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->event_height = static_cast<float>(height);
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
} 

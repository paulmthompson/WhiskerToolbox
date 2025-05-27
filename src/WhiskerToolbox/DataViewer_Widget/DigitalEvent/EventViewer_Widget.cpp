#include "EventViewer_Widget.hpp"
#include "ui_EventViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

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
}

EventViewer_Widget::~EventViewer_Widget() {
    delete ui;
}

void EventViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    // Set the color picker to the current color from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(key);
        if (config.has_value()) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
        } else {
            ui->color_picker->setColor("#FF0000"); // Default red
            ui->color_picker->setAlpha(100); // Default to full opacity
        }
    }
    
    std::cout << "EventViewer_Widget: Active key set to " << key << std::endl;
}

void EventViewer_Widget::_setEventColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalEventConfig(_active_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color.toStdString();
            _opengl_widget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
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
            _opengl_widget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
        }
        emit alphaChanged(_active_key, alpha_float);
    }
} 
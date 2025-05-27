#include "AnalogViewer_Widget.hpp"
#include "ui_AnalogViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <iostream>

AnalogViewer_Widget::AnalogViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
    
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &AnalogViewer_Widget::_setAnalogColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &AnalogViewer_Widget::_setAnalogAlpha);
}

AnalogViewer_Widget::~AnalogViewer_Widget() {
    delete ui;
}

void AnalogViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    // Set the color picker to the current color from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(key);
        if (config.has_value()) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
        } else {
            ui->color_picker->setColor("#0000FF"); // Default blue
            ui->color_picker->setAlpha(100); // Default to full opacity
        }
    }
    
    std::cout << "AnalogViewer_Widget: Active key set to " << key << std::endl;
}

void AnalogViewer_Widget::_setAnalogColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color.toStdString();
            _opengl_widget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
        }
        emit colorChanged(_active_key, hex_color.toStdString());
    }
}

void AnalogViewer_Widget::_setAnalogAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->alpha = alpha_float;
            _opengl_widget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
        }
        emit alphaChanged(_active_key, alpha_float);
    }
} 
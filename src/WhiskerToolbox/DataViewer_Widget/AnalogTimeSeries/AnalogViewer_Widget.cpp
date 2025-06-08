#include "AnalogViewer_Widget.hpp"
#include "ui_AnalogViewer_Widget.h"

#include "../../DataViewer/DisplayOptions/TimeSeriesDisplayOptions.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
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
    connect(ui->scale_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogViewer_Widget::_setAnalogScaleFactor);
    connect(ui->line_thickness_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AnalogViewer_Widget::_setLineThickness);
    
    // Connect gap handling controls
    connect(ui->gap_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogViewer_Widget::_setGapHandlingMode);
    connect(ui->gap_threshold_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogViewer_Widget::_setGapThreshold);
}

AnalogViewer_Widget::~AnalogViewer_Widget() {
    delete ui;
}

void AnalogViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    // Set the color picker and scale factor to current values from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(key);
        if (config.has_value()) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
            
            // Set scale factor from user-friendly scale
            ui->scale_spinbox->setValue(static_cast<double>(config.value()->user_scale_factor));
            
            // Set line thickness
            ui->line_thickness_spinbox->setValue(config.value()->line_thickness);
            
            // Set gap handling controls
            ui->gap_mode_combo->setCurrentIndex(static_cast<int>(config.value()->gap_handling));
            ui->gap_threshold_spinbox->setValue(static_cast<double>(config.value()->gap_threshold));
        } else {
            ui->color_picker->setColor("#0000FF"); // Default blue
            ui->color_picker->setAlpha(100); // Default to full opacity
            ui->scale_spinbox->setValue(1.0); // Default scale
            ui->line_thickness_spinbox->setValue(1); // Default line thickness
            ui->gap_mode_combo->setCurrentIndex(0); // Default to AlwaysConnect
            ui->gap_threshold_spinbox->setValue(5.0); // Default threshold
        }
    }
    
    std::cout << "AnalogViewer_Widget: Active key set to " << key << std::endl;
}

void AnalogViewer_Widget::_setAnalogColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color.toStdString();
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
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
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
        emit alphaChanged(_active_key, alpha_float);
    }
}

void AnalogViewer_Widget::_setAnalogScaleFactor(double scale_factor) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            // Set the user-friendly scale factor directly
            config.value()->user_scale_factor = static_cast<float>(scale_factor);
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
}

void AnalogViewer_Widget::_setLineThickness(int thickness) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->line_thickness = thickness;
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
}

void AnalogViewer_Widget::_setGapHandlingMode(int mode_index) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->gap_handling = static_cast<AnalogGapHandling>(mode_index);
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
}

void AnalogViewer_Widget::_setGapThreshold(double threshold) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->gap_threshold = static_cast<float>(threshold);
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
    }
} 

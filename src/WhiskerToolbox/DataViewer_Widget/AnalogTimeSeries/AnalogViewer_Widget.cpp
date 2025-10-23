#include "AnalogViewer_Widget.hpp"
#include "ui_AnalogViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <QColorDialog>
#include <iostream>

AnalogViewer_Widget::AnalogViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
    
    // Set the color display button to be flat and show just the color
    ui->color_display_button->setFlat(false);
    ui->color_display_button->setEnabled(false); // Make it non-clickable, just for display
    
    connect(ui->color_button, &QPushButton::clicked,
            this, &AnalogViewer_Widget::_openColorDialog);
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
    
    // Set the color and scale factor to current values from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(key);
        if (config.has_value()) {
            _updateColorDisplay(QString::fromStdString(config.value()->hex_color));
            
            // Set scale factor from the scaling config (not legacy user_scale_factor)
            ui->scale_spinbox->setValue(static_cast<double>(config.value()->scaling.user_scale_factor));
            
            // Set line thickness
            ui->line_thickness_spinbox->setValue(config.value()->line_thickness);
            
            // Set gap handling controls
            ui->gap_mode_combo->setCurrentIndex(static_cast<int>(config.value()->gap_handling));
            ui->gap_threshold_spinbox->setValue(static_cast<double>(config.value()->gap_threshold));
        } else {
            _updateColorDisplay("#0000FF"); // Default blue
            ui->scale_spinbox->setValue(1.0); // Default scale
            ui->line_thickness_spinbox->setValue(1); // Default line thickness
            ui->gap_mode_combo->setCurrentIndex(0); // Default to AlwaysConnect
            ui->gap_threshold_spinbox->setValue(5.0); // Default threshold
        }
    }
    
    std::cout << "AnalogViewer_Widget: Active key set to " << key << std::endl;
}

void AnalogViewer_Widget::_openColorDialog() {
    if (_active_key.empty()) {
        return;
    }
    
    // Get current color
    QColor currentColor;
    auto config = _opengl_widget->getAnalogConfig(_active_key);
    if (config.has_value()) {
        currentColor = QColor(QString::fromStdString(config.value()->hex_color));
    } else {
        currentColor = QColor("#0000FF");
    }
    
    // Open color dialog
    QColor color = QColorDialog::getColor(currentColor, this, "Choose Color");
    
    if (color.isValid()) {
        QString hex_color = color.name();
        _updateColorDisplay(hex_color);
        _setAnalogColor(hex_color);
    }
}

void AnalogViewer_Widget::_updateColorDisplay(QString const & hex_color) {
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
        QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color)
    );
}

void AnalogViewer_Widget::_setAnalogColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color.toStdString();
            emit colorChanged(_active_key, hex_color.toStdString());
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setAnalogAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->alpha = alpha_float;
            emit alphaChanged(_active_key, alpha_float);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setAnalogScaleFactor(double scale_factor) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            // Update the scaling config (the actual one used in rendering)
            config.value()->scaling.user_scale_factor = static_cast<float>(scale_factor);
            // Also update legacy field for compatibility
            config.value()->user_scale_factor = static_cast<float>(scale_factor);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setLineThickness(int thickness) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->line_thickness = thickness;
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setGapHandlingMode(int mode_index) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->gap_handling = static_cast<AnalogGapHandling>(mode_index);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setGapThreshold(double threshold) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getAnalogConfig(_active_key);
        if (config.has_value()) {
            config.value()->gap_threshold = static_cast<float>(threshold);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
} 

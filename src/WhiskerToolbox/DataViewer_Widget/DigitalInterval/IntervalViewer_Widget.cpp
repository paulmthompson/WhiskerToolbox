#include "IntervalViewer_Widget.hpp"
#include "ui_IntervalViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer/DisplayOptions/TimeSeriesDisplayOptions.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <iostream>
#include <QShowEvent>
#include <QHideEvent>

IntervalViewer_Widget::IntervalViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::IntervalViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
    
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &IntervalViewer_Widget::_setIntervalColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &IntervalViewer_Widget::_setIntervalAlpha);
}

IntervalViewer_Widget::~IntervalViewer_Widget() {
    delete ui;
}

void IntervalViewer_Widget::showEvent(QShowEvent * event) {
    std::cout << "IntervalViewer_Widget: Show Event" << std::endl;
    connect(_opengl_widget, &OpenGLWidget::mouseClick, this, &IntervalViewer_Widget::_selectInterval);
    QWidget::showEvent(event);
}

void IntervalViewer_Widget::hideEvent(QHideEvent * event) {
    std::cout << "IntervalViewer_Widget: Hide Event" << std::endl;
    disconnect(_opengl_widget, &OpenGLWidget::mouseClick, this, &IntervalViewer_Widget::_selectInterval);
    
    // Clear any selected interval when widget is hidden
    if (!_active_key.empty()) {
        _opengl_widget->clearSelectedInterval(_active_key);
    }
    
    QWidget::hideEvent(event);
}

void IntervalViewer_Widget::setActiveKey(std::string const & key) {
    // Clear previous selection if we had one
    if (!_active_key.empty()) {
        _opengl_widget->clearSelectedInterval(_active_key);
    }
    
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    _selection_enabled = !key.empty();
    
    // Set the color picker to the current color from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getDigitalIntervalConfig(key);
        if (config.has_value()) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
        } else {
            ui->color_picker->setColor("#00FF00"); // Default green
            ui->color_picker->setAlpha(100); // Default to full opacity
        }
    }
    
    std::cout << "IntervalViewer_Widget: Active key set to " << key << std::endl;
}

void IntervalViewer_Widget::_selectInterval(float time_coordinate, float canvas_y, QString const & series_info) {
    if (!_selection_enabled || _active_key.empty()) {
        return;
    }
    
    // Find interval at the clicked time coordinate
    auto interval = _opengl_widget->findIntervalAtTime(_active_key, time_coordinate);
    if (interval.has_value()) {
        auto const [start_time, end_time] = interval.value();
        
        std::cout << "IntervalViewer_Widget: Selected interval from " 
                  << start_time << " to " << end_time 
                  << " for series " << _active_key << std::endl;
        
        // Set the selected interval for highlighting
        _opengl_widget->setSelectedInterval(_active_key, start_time, end_time);
    } else {
        // No interval found at this time - clear selection
        _opengl_widget->clearSelectedInterval(_active_key);
    }
}

void IntervalViewer_Widget::_setIntervalColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto config = _opengl_widget->getDigitalIntervalConfig(_active_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color.toStdString();
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
        emit colorChanged(_active_key, hex_color.toStdString());
    }
}

void IntervalViewer_Widget::_setIntervalAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        auto config = _opengl_widget->getDigitalIntervalConfig(_active_key);
        if (config.has_value()) {
            config.value()->alpha = alpha_float;
            _opengl_widget->updateCanvas(_data_manager->getCurrentTime());
        }
        emit alphaChanged(_active_key, alpha_float);
    }
} 

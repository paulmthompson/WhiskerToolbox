#include "IntervalViewer_Widget.hpp"
#include "ui_IntervalViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <QColorDialog>
#include <QHideEvent>
#include <QShowEvent>
#include <iostream>

IntervalViewer_Widget::IntervalViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::IntervalViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
    
    // Set the color display button to be flat and show just the color
    ui->color_display_button->setFlat(false);
    ui->color_display_button->setEnabled(false); // Make it non-clickable, just for display
    
    connect(ui->color_button, &QPushButton::clicked,
            this, &IntervalViewer_Widget::_openColorDialog);
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
    
    // Set the color to the current color from display options if available
    if (!key.empty()) {
        auto config = _opengl_widget->getDigitalIntervalConfig(key);
        if (config.has_value()) {
            _updateColorDisplay(QString::fromStdString(config.value()->hex_color));
        } else {
            _updateColorDisplay("#00FF00"); // Default green
        }
    }
    
    std::cout << "IntervalViewer_Widget: Active key set to " << key << std::endl;
}

void IntervalViewer_Widget::_selectInterval(float time_coordinate, float canvas_y, QString const & series_info) {

    static_cast<void>(canvas_y);
    static_cast<void>(series_info);

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

void IntervalViewer_Widget::_openColorDialog() {
    if (_active_key.empty()) {
        return;
    }
    
    // Get current color
    QColor currentColor;
    auto config = _opengl_widget->getDigitalIntervalConfig(_active_key);
    if (config.has_value()) {
        currentColor = QColor(QString::fromStdString(config.value()->hex_color));
    } else {
        currentColor = QColor("#00FF00");
    }
    
    // Open color dialog
    QColor color = QColorDialog::getColor(currentColor, this, "Choose Color");
    
    if (color.isValid()) {
        QString hex_color = color.name();
        _updateColorDisplay(hex_color);
        _setIntervalColor(hex_color);
    }
}

void IntervalViewer_Widget::_updateColorDisplay(QString const & hex_color) {
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
        QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color)
    );
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

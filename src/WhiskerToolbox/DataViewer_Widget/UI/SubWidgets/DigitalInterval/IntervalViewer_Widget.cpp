#include "IntervalViewer_Widget.hpp"
#include "ui_IntervalViewer_Widget.h"

#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "DataViewer_Widget/Rendering/OpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"

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
    // Selection is now handled directly in OpenGLWidget::mousePressEvent via hit testing
    // No need to connect to signals here - EntityId-based selection is automatic
    QWidget::showEvent(event);
}

void IntervalViewer_Widget::hideEvent(QHideEvent * event) {
    std::cout << "IntervalViewer_Widget: Hide Event" << std::endl;
    
    // Clear all selected entities when widget is hidden
    _opengl_widget->clearEntitySelection();
    
    QWidget::hideEvent(event);
}

void IntervalViewer_Widget::setActiveKey(std::string const & key) {
    // Clear previous selection if we had one
    if (!_active_key.empty()) {
        _opengl_widget->clearEntitySelection();
    }
    
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    _selection_enabled = !key.empty();
    
    // Set the color to the current color from state if available
    if (!key.empty()) {
        auto const * opts = _opengl_widget->state()->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            _updateColorDisplay(QString::fromStdString(opts->hex_color()));
        } else {
            _updateColorDisplay("#00FF00"); // Default green
        }
    }
    
    std::cout << "IntervalViewer_Widget: Active key set to " << key << std::endl;
}

// Note: _selectInterval has been removed - interval selection is now handled
// directly in OpenGLWidget::mousePressEvent via hitTestAtPosition() and EntityId-based
// selection API (selectEntity, deselectEntity, toggleEntitySelection)

void IntervalViewer_Widget::_openColorDialog() {
    if (_active_key.empty()) {
        return;
    }
    
    // Get current color
    QColor currentColor;
    auto const * opts = _opengl_widget->state()->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(_active_key));
    if (opts) {
        currentColor = QColor(QString::fromStdString(opts->hex_color()));
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
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->hex_color() = hex_color.toStdString();
            emit colorChanged(_active_key, hex_color.toStdString());
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void IntervalViewer_Widget::_setIntervalAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->alpha() = alpha_float;
            emit alphaChanged(_active_key, alpha_float);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
} 

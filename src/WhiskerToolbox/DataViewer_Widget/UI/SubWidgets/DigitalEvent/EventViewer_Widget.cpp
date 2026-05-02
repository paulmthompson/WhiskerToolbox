#include "EventViewer_Widget.hpp"
#include "ui_EventViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "DataViewer_Widget/Rendering/OpenGLWidget.hpp"

#include <QColorDialog>
#include <QSignalBlocker>

#include <iostream>

EventViewer_Widget::EventViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget} {
    ui->setupUi(this);

    // Set the color display button to be flat and show just the color
    ui->color_display_button->setFlat(false);
    ui->color_display_button->setEnabled(false);// Make it non-clickable, just for display

    connect(ui->color_button, &QPushButton::clicked,
            this, &EventViewer_Widget::_openColorDialog);
    connect(ui->alpha_slider, &QSlider::valueChanged,
            this, &EventViewer_Widget::_setEventAlpha);

    // Connect display option controls
    connect(ui->mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventViewer_Widget::_setDisplayMode);
    connect(ui->spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventViewer_Widget::_setVerticalSpacing);
    connect(ui->height_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventViewer_Widget::_setEventHeight);
    connect(ui->glyph_shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventViewer_Widget::_setGlyphShape);
    connect(ui->box_width_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EventViewer_Widget::_setBoxWidth);

    _updateBoxWidthControlsForGlyphShape(EventGlyphShapeData::Tick);
}

EventViewer_Widget::~EventViewer_Widget() {
    delete ui;
}

void EventViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color and display options to current values from state if available
    if (!key.empty()) {
        auto const * opts = _opengl_widget->state()->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            _updateColorDisplay(QString::fromStdString(opts->hex_color()));

            // Set alpha from state
            auto const alpha_int = static_cast<int>(opts->get_alpha() * 100.0f);
            ui->alpha_slider->setValue(alpha_int);
            ui->alpha_value_label->setText(QString::number(static_cast<double>(opts->get_alpha()), 'f', 2));

            // Set display mode controls
            ui->mode_combo->setCurrentIndex(static_cast<int>(opts->plotting_mode));
            ui->spacing_spinbox->setValue(static_cast<double>(opts->vertical_spacing));
            ui->height_spinbox->setValue(static_cast<double>(opts->event_height));

            QSignalBlocker const glyph_block{ui->glyph_shape_combo};
            QSignalBlocker const box_block{ui->box_width_spinbox};
            ui->glyph_shape_combo->setCurrentIndex(static_cast<int>(opts->glyph_shape));
            ui->box_width_spinbox->setValue(opts->box_width_ticks);
            _updateBoxWidthControlsForGlyphShape(opts->glyph_shape);
        } else {
            _updateColorDisplay("#FF0000");    // Default red
            ui->mode_combo->setCurrentIndex(0);// Default to Stacked
            ui->spacing_spinbox->setValue(0.1);// Default spacing
            ui->height_spinbox->setValue(0.05);// Default height
            QSignalBlocker const glyph_block{ui->glyph_shape_combo};
            QSignalBlocker const box_block{ui->box_width_spinbox};
            ui->glyph_shape_combo->setCurrentIndex(static_cast<int>(EventGlyphShapeData::Tick));
            ui->box_width_spinbox->setValue(10);
            _updateBoxWidthControlsForGlyphShape(EventGlyphShapeData::Tick);
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
    auto const * opts = _opengl_widget->state()->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
    if (opts) {
        currentColor = QColor(QString::fromStdString(opts->hex_color()));
    } else {
        currentColor = QColor("#FF0000");
    }

    // Open color dialog
    QColor const color = QColorDialog::getColor(currentColor, this, "Choose Color");

    if (color.isValid()) {
        QString const hex_color = color.name();
        _updateColorDisplay(hex_color);
        _setEventColor(hex_color);
    }
}

void EventViewer_Widget::_updateColorDisplay(QString const & hex_color) {
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void EventViewer_Widget::_setEventColor(QString const & hex_color) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->hex_color() = hex_color.toStdString();
            emit colorChanged(_active_key, hex_color.toStdString());
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setEventAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        ui->alpha_value_label->setText(QString::number(static_cast<double>(alpha_float), 'f', 2));
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->alpha() = alpha_float;
            emit alphaChanged(_active_key, alpha_float);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setDisplayMode(int mode_index) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->plotting_mode = static_cast<EventPlottingModeData>(mode_index);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setVerticalSpacing(double spacing) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->vertical_spacing = static_cast<float>(spacing);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setEventHeight(double height) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->event_height = static_cast<float>(height);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void EventViewer_Widget::_setGlyphShape(int const shape_index) {
    if (_active_key.empty()) {
        return;
    }
    if (shape_index < 0 || shape_index > static_cast<int>(EventGlyphShapeData::Box)) {
        return;
    }
    auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
    if (!opts) {
        return;
    }
    opts->glyph_shape = static_cast<EventGlyphShapeData>(shape_index);
    _updateBoxWidthControlsForGlyphShape(opts->glyph_shape);
    _opengl_widget->update();
}

void EventViewer_Widget::_setBoxWidth(int const ticks) {
    if (_active_key.empty()) {
        return;
    }
    auto * opts = _opengl_widget->state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(_active_key));
    if (!opts || opts->glyph_shape != EventGlyphShapeData::Box) {
        return;
    }
    opts->box_width_ticks = ticks;
    _opengl_widget->update();
}

/**
 * @brief Toggle visibility of box-width controls to match the selected marker shape
 */
void EventViewer_Widget::_updateBoxWidthControlsForGlyphShape(EventGlyphShapeData const shape) {
    bool const show_box_width = (shape == EventGlyphShapeData::Box);
    ui->box_width_label->setVisible(show_box_width);
    ui->box_width_spinbox->setVisible(show_box_width);
    ui->box_width_spinbox->setEnabled(show_box_width);
}

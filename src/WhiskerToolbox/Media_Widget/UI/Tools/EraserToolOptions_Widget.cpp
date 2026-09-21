/**
 * @file EraserToolOptions_Widget.cpp
 * @brief Options bar page for the Media Viewer Eraser tool
 */

#include "EraserToolOptions_Widget.hpp"
#include "ToolOptionsLayoutHelpers.hpp"
#include "ui_EraserToolOptions_Widget.h"

#include "Core/MediaWidgetState.hpp"

#include <QSlider>
#include <QSpinBox>

EraserToolOptions_Widget::EraserToolOptions_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EraserToolOptions_Widget) {
    ui->setupUi(this);
    configureShrinkableToolOptionsPage(this);
    configureCompactInstructionLabel(ui->instruction_label);

    connect(ui->eraser_size_slider, &QSlider::valueChanged,
            ui->eraser_size_spinbox, &QSpinBox::setValue);
    connect(ui->eraser_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->eraser_size_slider, &QSlider::setValue);
    connect(ui->eraser_size_slider, &QSlider::valueChanged,
            this, &EraserToolOptions_Widget::_onEraserSizeChanged);
}

EraserToolOptions_Widget::~EraserToolOptions_Widget() {
    delete ui;
}

void EraserToolOptions_Widget::setState(MediaWidgetState * state) {
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;

    if (!_state) {
        return;
    }

    connect(_state, &MediaWidgetState::eraserPrefsChanged,
            this, &EraserToolOptions_Widget::_syncFromState);

    _syncFromState();
}

void EraserToolOptions_Widget::_onEraserSizeChanged(int radius_px) {
    if (_updating_from_state || !_state) {
        return;
    }

    EraserToolPrefs prefs = _state->eraserPrefs();
    prefs.radius_px = radius_px;
    _state->setEraserPrefs(prefs);
}

void EraserToolOptions_Widget::_syncFromState() {
    if (!_state) {
        return;
    }

    _updating_from_state = true;
    ui->eraser_size_slider->setValue(_state->eraserPrefs().radius_px);
    _updating_from_state = false;
}

/**
 * @file SmoothToolOptions_Widget.cpp
 * @brief Options bar page for the Media Viewer Smooth tool
 */

#include "SmoothToolOptions_Widget.hpp"
#include "ui_SmoothToolOptions_Widget.h"

#include "Core/MediaWidgetState.hpp"

#include <QSlider>
#include <QSpinBox>

SmoothToolOptions_Widget::SmoothToolOptions_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::SmoothToolOptions_Widget) {
    ui->setupUi(this);

    connect(ui->smooth_size_slider, &QSlider::valueChanged,
            ui->smooth_size_spinbox, &QSpinBox::setValue);
    connect(ui->smooth_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->smooth_size_slider, &QSlider::setValue);
    connect(ui->smooth_size_slider, &QSlider::valueChanged,
            this, &SmoothToolOptions_Widget::_onSmoothSizeChanged);
}

SmoothToolOptions_Widget::~SmoothToolOptions_Widget() {
    delete ui;
}

void SmoothToolOptions_Widget::setState(MediaWidgetState * state) {
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;

    if (!_state) {
        return;
    }

    connect(_state, &MediaWidgetState::smoothPrefsChanged,
            this, &SmoothToolOptions_Widget::_syncFromState);

    _syncFromState();
}

void SmoothToolOptions_Widget::_onSmoothSizeChanged(int radius_px) {
    if (_updating_from_state || !_state) {
        return;
    }

    SmoothToolPrefs prefs = _state->smoothPrefs();
    prefs.radius_px = radius_px;
    _state->setSmoothPrefs(prefs);
}

void SmoothToolOptions_Widget::_syncFromState() {
    if (!_state) {
        return;
    }

    _updating_from_state = true;
    ui->smooth_size_slider->setValue(_state->smoothPrefs().radius_px);
    _updating_from_state = false;
}

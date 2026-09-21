/**
 * @file SmoothToolOptions_Widget.cpp
 * @brief Options bar page for the Media Viewer Smooth tool
 */

#include "SmoothToolOptions_Widget.hpp"
#include "ToolOptionsLayoutHelpers.hpp"
#include "ui_SmoothToolOptions_Widget.h"

#include "Core/LineEditOperations.hpp"
#include "Core/MediaWidgetState.hpp"

#include <QComboBox>
#include <QSlider>
#include <QSpinBox>

namespace {

[[nodiscard]] int indexForAlgorithm(LineSmoothAlgorithm algorithm) {
    switch (algorithm) {
        case LineSmoothAlgorithm::MovingAverage:
            return 0;
        case LineSmoothAlgorithm::PolynomialFit:
            return 1;
    }
    return 0;
}

[[nodiscard]] LineSmoothAlgorithm algorithmForIndex(int index) {
    switch (index) {
        case 1:
            return LineSmoothAlgorithm::PolynomialFit;
        case 0:
        default:
            return LineSmoothAlgorithm::MovingAverage;
    }
}

}// namespace

SmoothToolOptions_Widget::SmoothToolOptions_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::SmoothToolOptions_Widget) {
    ui->setupUi(this);
    configureShrinkableToolOptionsPage(this);
    configureCompactInstructionLabel(ui->instruction_label);

    connect(ui->smooth_size_slider, &QSlider::valueChanged,
            ui->smooth_size_spinbox, &QSpinBox::setValue);
    connect(ui->smooth_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->smooth_size_slider, &QSlider::setValue);
    connect(ui->smooth_size_slider, &QSlider::valueChanged,
            this, &SmoothToolOptions_Widget::_onSmoothSizeChanged);

    connect(ui->strength_slider, &QSlider::valueChanged,
            ui->strength_spinbox, &QSpinBox::setValue);
    connect(ui->strength_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->strength_slider, &QSlider::setValue);
    connect(ui->strength_slider, &QSlider::valueChanged,
            this, &SmoothToolOptions_Widget::_onStrengthChanged);

    connect(ui->algorithm_combo, &QComboBox::currentIndexChanged,
            this, &SmoothToolOptions_Widget::_onAlgorithmChanged);
    connect(ui->polynomial_order_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SmoothToolOptions_Widget::_onPolynomialOrderChanged);
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

void SmoothToolOptions_Widget::_onAlgorithmChanged(int index) {
    if (_updating_from_state || !_state) {
        return;
    }

    SmoothToolPrefs prefs = _state->smoothPrefs();
    prefs.algorithm = algorithmForIndex(index);
    _state->setSmoothPrefs(prefs);
    _updateAlgorithmDependentControls();
}

void SmoothToolOptions_Widget::_onStrengthChanged(int strength) {
    if (_updating_from_state || !_state) {
        return;
    }

    SmoothToolPrefs prefs = _state->smoothPrefs();
    prefs.strength = strength;
    _state->setSmoothPrefs(prefs);
}

void SmoothToolOptions_Widget::_onPolynomialOrderChanged(int order) {
    if (_updating_from_state || !_state) {
        return;
    }

    SmoothToolPrefs prefs = _state->smoothPrefs();
    prefs.polynomial_order = order;
    _state->setSmoothPrefs(prefs);
}

void SmoothToolOptions_Widget::_syncFromState() {
    if (!_state) {
        return;
    }

    SmoothToolPrefs const prefs = _state->smoothPrefs();

    _updating_from_state = true;
    ui->smooth_size_slider->setValue(prefs.radius_px);
    ui->algorithm_combo->setCurrentIndex(indexForAlgorithm(prefs.algorithm));
    ui->strength_slider->setValue(prefs.strength);
    ui->polynomial_order_spinbox->setValue(prefs.polynomial_order);
    _updating_from_state = false;

    _updateAlgorithmDependentControls();
}

void SmoothToolOptions_Widget::_updateAlgorithmDependentControls() {
    bool const polynomial_fit =
            _state != nullptr &&
            _state->smoothPrefs().algorithm == LineSmoothAlgorithm::PolynomialFit;

    ui->polynomial_order_label->setEnabled(polynomial_fit);
    ui->polynomial_order_spinbox->setEnabled(polynomial_fit);

    bool const moving_average =
            _state != nullptr &&
            _state->smoothPrefs().algorithm == LineSmoothAlgorithm::MovingAverage;

    ui->strength_label->setEnabled(moving_average);
    ui->strength_slider->setEnabled(moving_average);
    ui->strength_spinbox->setEnabled(moving_average);
}

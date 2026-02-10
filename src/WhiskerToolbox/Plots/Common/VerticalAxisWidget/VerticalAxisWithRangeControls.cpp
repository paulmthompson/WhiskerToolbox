#include "VerticalAxisWithRangeControls.hpp"

#include "VerticalAxisWidget.hpp"
#include "Core/VerticalAxisState.hpp"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

#include <cmath>

VerticalAxisRangeControls::VerticalAxisRangeControls(
    VerticalAxisState * state,
    QWidget * parent)
    : QWidget(parent),
      _state(state),
      _min_spinbox(nullptr),
      _max_spinbox(nullptr),
      _updating_ui(false)
{
    auto * layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // Min range label and spinbox
    auto * min_label = new QLabel("Min:", this);
    layout->addWidget(min_label);

    _min_spinbox = new QDoubleSpinBox(this);
    _min_spinbox->setMinimum(-1000000.0);
    _min_spinbox->setMaximum(1000000.0);
    _min_spinbox->setDecimals(1);
    _min_spinbox->setMinimumWidth(100);
    layout->addWidget(_min_spinbox);

    // Separator
    auto * separator = new QLabel("to", this);
    layout->addWidget(separator);

    // Max range label and spinbox
    auto * max_label = new QLabel("Max:", this);
    layout->addWidget(max_label);

    _max_spinbox = new QDoubleSpinBox(this);
    _max_spinbox->setMinimum(-1000000.0);
    _max_spinbox->setMaximum(1000000.0);
    _max_spinbox->setDecimals(1);
    _max_spinbox->setMinimumWidth(100);
    layout->addWidget(_max_spinbox);

    // Connect spinbox signals
    connect(_min_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VerticalAxisRangeControls::onMinRangeChanged);
    connect(_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VerticalAxisRangeControls::onMaxRangeChanged);

    // Connect to state updates (programmatic changes)
    if (_state) {
        connect(_state, &VerticalAxisState::rangeUpdated,
                this, &VerticalAxisRangeControls::onStateRangeUpdated);
        // Also connect to rangeChanged to handle user changes from other sources
        connect(_state, &VerticalAxisState::rangeChanged,
                this, &VerticalAxisRangeControls::onStateRangeChanged);
    }

    // Initialize spinboxes
    updateSpinBoxes();
}

void VerticalAxisRangeControls::onMinRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setYMin(value);
}

void VerticalAxisRangeControls::onMaxRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setYMax(value);
}

void VerticalAxisRangeControls::onStateRangeUpdated(double y_min, double y_max)
{
    Q_UNUSED(y_min)
    Q_UNUSED(y_max)
    updateSpinBoxes();
}

void VerticalAxisRangeControls::onStateRangeChanged(double y_min, double y_max)
{
    Q_UNUSED(y_min)
    Q_UNUSED(y_max)
    // Update spinboxes when range changes (could be from user input or programmatic)
    updateSpinBoxes();
}

void VerticalAxisRangeControls::updateSpinBoxes()
{
    if (!_state) {
        return;
    }

    _updating_ui = true;

    // Update spinboxes if they don't already have the correct value
    if (std::abs(_min_spinbox->value() - _state->getYMin()) > 0.01) {
        _min_spinbox->setValue(_state->getYMin());
    }
    if (std::abs(_max_spinbox->value() - _state->getYMax()) > 0.01) {
        _max_spinbox->setValue(_state->getYMax());
    }

    _updating_ui = false;
}

void VerticalAxisWithRangeControls::setRangeGetter(VerticalAxisWidget::RangeGetter getter)
{
    if (axis_widget) {
        axis_widget->setRangeGetter(std::move(getter));
    }
}

void VerticalAxisWithRangeControls::setRange(double min_range, double max_range)
{
    if (state) {
        state->setRange(min_range, max_range);
    }
}

void VerticalAxisWithRangeControls::getRange(double & min_range, double & max_range) const
{
    if (state) {
        min_range = state->getYMin();
        max_range = state->getYMax();
    } else {
        min_range = 0.0;
        max_range = 100.0;
    }
}

VerticalAxisWithRangeControls createVerticalAxisWithRangeControls(
    VerticalAxisState * state,
    QWidget * axis_parent,
    QWidget * controls_parent)
{
    VerticalAxisWithRangeControls result;
    result.state = state;

    // Create axis widget
    result.axis_widget = new VerticalAxisWidget(axis_parent);

    // Set up axis widget to read from state
    if (state) {
        result.axis_widget->setRangeGetter([state]() {
            if (!state) {
                return std::make_pair(0.0, 100.0);
            }
            return std::make_pair(state->getYMin(), state->getYMax());
        });

        // Connect axis widget to state changes for repainting
        QObject::connect(state, &VerticalAxisState::rangeChanged,
                        result.axis_widget, [widget = result.axis_widget](double /* y_min */, double /* y_max */) {
                            if (widget) {
                                widget->update();
                            }
                        });
        QObject::connect(state, &VerticalAxisState::rangeUpdated,
                        result.axis_widget, [widget = result.axis_widget](double /* y_min */, double /* y_max */) {
                            if (widget) {
                                widget->update();
                            }
                        });
    }

    // Create range controls widget
    result.range_controls = new VerticalAxisRangeControls(state, controls_parent);

    return result;
}
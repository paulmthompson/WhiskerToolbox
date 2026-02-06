#include "HorizontalAxisWithRangeControls.hpp"

#include "HorizontalAxisWidget.hpp"
#include "Core/HorizontalAxisState.hpp"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

#include <cmath>

HorizontalAxisRangeControls::HorizontalAxisRangeControls(
    HorizontalAxisState * state,
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
            this, &HorizontalAxisRangeControls::onMinRangeChanged);
    connect(_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &HorizontalAxisRangeControls::onMaxRangeChanged);

    // Connect to state updates (programmatic changes)
    if (_state) {
        connect(_state, &HorizontalAxisState::rangeUpdated,
                this, &HorizontalAxisRangeControls::onStateRangeUpdated);
        // Also connect to rangeChanged to handle user changes from other sources
        connect(_state, &HorizontalAxisState::rangeChanged,
                this, &HorizontalAxisRangeControls::onStateRangeChanged);
    }

    // Initialize spinboxes
    updateSpinBoxes();
}

void HorizontalAxisRangeControls::onMinRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setXMin(value);
}

void HorizontalAxisRangeControls::onMaxRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setXMax(value);
}

void HorizontalAxisRangeControls::onStateRangeUpdated(double x_min, double x_max)
{
    Q_UNUSED(x_min)
    Q_UNUSED(x_max)
    updateSpinBoxes();
}

void HorizontalAxisRangeControls::onStateRangeChanged(double x_min, double x_max)
{
    Q_UNUSED(x_min)
    Q_UNUSED(x_max)
    // Update spinboxes when range changes (could be from user input or programmatic)
    updateSpinBoxes();
}

void HorizontalAxisRangeControls::updateSpinBoxes()
{
    if (!_state) {
        return;
    }

    _updating_ui = true;

    // Update spinboxes if they don't already have the correct value
    if (std::abs(_min_spinbox->value() - _state->getXMin()) > 0.01) {
        _min_spinbox->setValue(_state->getXMin());
    }
    if (std::abs(_max_spinbox->value() - _state->getXMax()) > 0.01) {
        _max_spinbox->setValue(_state->getXMax());
    }

    _updating_ui = false;
}

void HorizontalAxisWithRangeControls::setRangeGetter(HorizontalAxisWidget::RangeGetter getter)
{
    if (axis_widget) {
        axis_widget->setRangeGetter(std::move(getter));
    }
}

void HorizontalAxisWithRangeControls::setRange(double min_range, double max_range)
{
    if (state) {
        state->setRange(min_range, max_range);
    }
}

void HorizontalAxisWithRangeControls::getRange(double & min_range, double & max_range) const
{
    if (state) {
        min_range = state->getXMin();
        max_range = state->getXMax();
    } else {
        min_range = 0.0;
        max_range = 100.0;
    }
}

HorizontalAxisWithRangeControls createHorizontalAxisWithRangeControls(
    HorizontalAxisState * state,
    QWidget * axis_parent,
    QWidget * controls_parent)
{
    HorizontalAxisWithRangeControls result;
    result.state = state;

    // Create axis widget
    result.axis_widget = new HorizontalAxisWidget(axis_parent);

    // Set up axis widget to read from state
    if (state) {
        result.axis_widget->setRangeGetter([state]() {
            if (!state) {
                return std::make_pair(0.0, 100.0);
            }
            return std::make_pair(state->getXMin(), state->getXMax());
        });

        // Connect axis widget to state changes for repainting
        QObject::connect(state, &HorizontalAxisState::rangeChanged,
                        result.axis_widget, [widget = result.axis_widget](double /* x_min */, double /* x_max */) {
                            if (widget) {
                                widget->update();
                            }
                        });
        QObject::connect(state, &HorizontalAxisState::rangeUpdated,
                        result.axis_widget, [widget = result.axis_widget](double /* x_min */, double /* x_max */) {
                            if (widget) {
                                widget->update();
                            }
                        });
    }

    // Create range controls widget
    result.range_controls = new HorizontalAxisRangeControls(state, controls_parent);

    return result;
}
